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

#include "thread.h"
#include <poll.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int loop_thread_process_idle(thread_params_t *tp) {
	int ret = IDLE_CONTINUE_PROC;
	pthread_mutex_lock(&tp->sq.lock); {
		queue_elem_t* p = tp->sq.head;
		while(p && p->elem) {
			client_params_t *cp = p->elem;
			if(cp->cmd_type == COMMAND_TYPE_ADMIN) {
				if(cp->command_adm==COMMAND_TERMINATE_THREAD) {
					ret = IDLE_TERMINATE;
					goto finished;
				}
			}
		}
	}
finished:
	pthread_mutex_unlock(&tp->sq.lock);
	return ret;
}

void* thread_loop(void *v) {
	thread_params_t *tp = v;
	init_queue(&tp->sq);
	init_queue(&tp->eq);
	int port = tp->fd;
	unsigned char buf[THREAD_RECEIVE_BUFSIZE];
	size_t offset=0;
	size_t total=0;
	struct pollfd fds[1];

	if(tp->thread_created_notify)
		tp->thread_created_notify(tp);

	fds[0].fd = port;
	fds[0].events = POLLIN | POLLERR | POLLHUP | POLLNVAL;
	for(;;) {
		int pollret = poll(fds, 1, tp->timeout_msec);
		if(pollret>0) {
			if(fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
				goto error;
			else if(tp->notifyonly) {
				tp->thread_process_input(tp, NULL, 0);
			} else {
				int ret;
				ret=read(port, buf+offset, THREAD_RECEIVE_BUFSIZE-offset);
				if(ret>0) {
					total+=ret; // port statistics
					buf[offset+ret]=0; // for text interfaces
					if(tp->thread_process_input) {
						size_t consumed = tp->thread_process_input(tp, buf, offset+ret);
						offset = offset+ret-consumed;
						if(consumed)
							memmove(buf, buf+consumed, offset);
					}
				}
			}
		} else if(pollret==0) {
			//int ret = loop_thread_process_idle(tp); // make sure that the mutex and the rest are initialized
			int ret = IDLE_CONTINUE_PROC;
			if(ret==IDLE_CONTINUE_PROC && tp->thread_process_idle)
				ret = tp->thread_process_idle(tp);
			if(ret==IDLE_TERMINATE)
				goto quit;
		} else
			goto error;
	}
quit:
	DBGT("regular exit. total bytes received: %lu", total)
	tp->retval = 0;
	goto end;
error:
	// errno is thread-local, therefore thread safe, according to POSIX.1
	DBGT("exit on error. total bytes received: %lu", total)
	tp->retval = 1;
	goto end;
end:
	destroy_queue(&tp->sq);
	destroy_queue(&tp->eq);
	if(tp->thread_exiting_notify)
		tp->thread_exiting_notify(tp);
	pthread_exit(&tp->retval);
	return &tp->retval; // should not be reached
}

int create_loop_thread(thread_params_t *tp) {
	memset(&tp->tid, 0, sizeof(pthread_t));
	return pthread_create(&tp->tid, NULL, thread_loop, tp);
}

void loop_thread_created(thread_params_t *tp) {
	DBGT()
}

void loop_thread_exiting(thread_params_t *tp) {
	DBGT()
	closeport(tp->fd, &tp->oldt);
}

client_params_t *new_client_thread(const char *name, thread_params_t *interface) {
	client_params_t *cp = calloc(1, sizeof(client_params_t));
	pthread_cond_init(&cp->waitcond, NULL); // initialize waiting condition
	pthread_mutex_init(&cp->waitmutex, NULL);
	strncpy(cp->name, name, sizeof(cp->name));
	cp->tp_interface = interface;
	memset(&cp->tid, 0, sizeof(pthread_t));
	return cp;
}

void send_command(void *cmd, client_params_t *cp) {
	cp->command = cmd;
	cp->status = COMMAND_STATE_WAIT_TO_SEND;
	pthread_mutex_lock(&cp->waitmutex);
	append_elem_to_queue(&cp->tp_interface->sq, cp);
	DBGC()
	pthread_cond_wait(&cp->waitcond, &cp->waitmutex);
	pthread_mutex_unlock(&cp->waitmutex);
}

void destroy_client_thread(client_params_t *cp) {
	pthread_cond_destroy(&cp->waitcond);
	pthread_mutex_destroy(&cp->waitmutex);
	free(cp);
}

void add_event_handler(thread_params_t *tp_interface, event_handler_t* eh) {
	append_elem_to_queue(&tp_interface->eq, eh);
}

void remove_event_handler(thread_params_t *tp_interface, event_handler_t* eh) {
	remove_elem_from_queue(&tp_interface->eq, eh);
}
