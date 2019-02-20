/*******************************************************************************
// software distributed under freeBSD license as follow

Copyright (c) 2018, Gemalto M2M
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the <project name> project.

*******************************************************************************/

#include "thread_at.h"
#include <string.h>
#include <stdlib.h>
#include <poll.h>
#include <unistd.h>

#define NUM_AT_TERMINATORS	(10)
const char *canonical_at_terminators[NUM_AT_TERMINATORS] = {
	"OK",
	"CONNECT",
	"NO CARRIER",
	"ERROR",
	"+CME ERROR:",
	"NO DIALTONE",
	"BUSY",
	"NO ANSWER",
	"+CMS ERROR:",
	"^SYSSTART"
	// AT^SBNW errors not included here
};

// returns consumed
size_t at_process_input(thread_params_t *tp, const unsigned char *buf, size_t size) {
	size_t consumed = 0;

	int incomplete_answer = 0;
	pthread_mutex_lock(&tp->sq.lock); {
		queue_elem_t* p = tp->sq.head;
		while(p && p->elem) {
			client_params_t *cp = p->elem;
			if(cp->status == COMMAND_STATE_WAIT_ANSWER) {
				int i;
				incomplete_answer = 1;
				// looking for a line like: \r\n<term>[^\r]*\r\n
				for(i=0;i<NUM_AT_TERMINATORS;i++) {
					const unsigned char *pos = buf;
					pos = strchr(pos, '\n');
					while(pos && (pos-buf+1)<size) {
						pos++;
						if(strncmp(pos, canonical_at_terminators[i], strlen(canonical_at_terminators[i]))==0) {
							size_t anslen;
							pos = strchr(pos, '\n');
							if(!pos || (pos-buf)>=size)
								goto finished;
							while(*pos == '\r' || *pos == '\n') // remove from the answer
								pos--;
							incomplete_answer = 0;
							anslen = pos-buf+1;
							cp->response = strndup(buf, anslen); // the string will be anslen+1 bytes with a null terminator
							// ready to process the rest
							buf+=anslen;
							size-=anslen;
							consumed+=anslen;
							// remove command from the list, unlock condition
							pthread_mutex_unlock((&tp->sq.lock));
							remove_elem_from_queue(&tp->sq, cp);
							pthread_mutex_lock((&tp->sq.lock));
							pthread_cond_signal(&cp->waitcond);
							goto finished;
						}
						pos = strchr(pos, '\n');
					}
				}
				break; // for AT commands, only 1 outstanding at a time
			}
		}
	}
finished:
	pthread_mutex_unlock(&tp->sq.lock);

	if(incomplete_answer)
		return consumed; // 0 for AT commands, since it is one at a time and no urc in the middle (for now)

	// skip blanks
	while(size && (*buf=='\n' || *buf=='\r')) {
		buf++;
		consumed++;
		size--;
	}
	// if a full line is present, it is an urc
	const unsigned char *pos=strchr(buf,'\n');
	while(size && pos) {
		size_t urc_len=pos-buf;
		char *urc = strndup(buf, urc_len);

		while(urc_len && buf[urc_len-1]=='\r') {
			urc[urc_len-1]=0;
			urc_len--;
		}
		DBGT("URC>'%s'", urc);
		free(urc);

		urc_len = pos-buf+1;
		buf+=urc_len;
		consumed+=urc_len;
		size-=urc_len;

		// skip blanks
		while(size && (*buf=='\n' || *buf=='\r')) {
			buf++;
			consumed++;
			size--;
		}
		pos=strchr(buf,'\n'); // look for the next
	}
	return consumed;
}

int at_process_idle(thread_params_t *tp) {
	pthread_mutex_lock(&tp->sq.lock); {
		queue_elem_t* p = tp->sq.head;
		while(p && p->elem) {
			client_params_t *cp = p->elem;
			if(cp->status != COMMAND_STATE_WAIT_TO_SEND) // status == COMMAND_STATE_WAIT_ANSWER
				goto finished; // for AT interface, no multiple sending
			else {
				struct pollfd fds[1];
				int pollret;
				fds[0].fd = tp->fd;
				fds[0].events = POLLOUT | POLLERR | POLLHUP | POLLNVAL;
				size_t bufsize = strlen(cp->command);
				size_t written = 0;
				while(written<bufsize) {
					pollret = poll(fds, 1, tp->timeout_msec);
					if(pollret>0) {
						if(fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
							goto finished; // do not process error here, the reading poll will do it
						written += write(tp->fd, cp->command+written, bufsize-written);
					} else if(pollret<0)
						goto finished;
					// else in case of timeout, just repeat
				}
				cp->status = COMMAND_STATE_WAIT_ANSWER;
				goto finished; // one command sending for cycle
			}
			p=p->next;
		}
	}
finished:
	pthread_mutex_unlock(&tp->sq.lock);
	return IDLE_FINISHED_PROC;
}

thread_params_t *create_at_thread(const char *portname) {
	thread_params_t *tp = (thread_params_t *)calloc(1, sizeof(thread_params_t));
	strncpy(tp->name, portname, sizeof(tp->name));
	tp->fd = openport(portname, &tp->oldt, &tp->newt);
	if(tp->fd<0)
		goto error;
	tp->timeout_msec = 100; // standard interval between at commands
	tp->thread_created_notify = loop_thread_created;
	tp->thread_exiting_notify = loop_thread_exiting;
	tp->thread_process_input = at_process_input;
	tp->thread_process_idle = at_process_idle;
	if(create_loop_thread(tp) != 0)
		goto error;
	return tp;
error:
	loop_thread_exiting(tp);
	free(tp);
	return NULL;
}
