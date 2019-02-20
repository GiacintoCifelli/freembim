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

#include "thread_mbim.h"
#include "mbim_lib.h"
#include "mbim_procs.h"
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <unistd.h>

void discard_current_frame(thread_params_t *tp) {
	if(!tp->current) // normal case
		return;
	uint32_t type = mbim_get_frame_msg_type(tp->current->data);
	if(type==MBIM_COMMAND_DONE) {
		DBGT("frame lost in command response!")
		// TODO: look for this command by sequence_id and unlock it with error
	} // otherwise for MBIM_INDICATE_STATUS_MSG there is nothing to do (and other msg types can't be multiframe)
	mbim_free_frame(tp->current);
	tp->current=NULL;
}

void process_mbim_frame(thread_params_t *tp, unsigned char *frame) {
	DBGT()
	print_mbim_frame(frame);
	mbim_function_message_t *msg;
	uint32_t header_len = sizeof(uint32_t)*5;

	// message fragmentation management. TODO: check that each fragment has the same type and sequence_id
	uint32_t fragments = mbim_get_frame_fragments(frame);
	if(fragments>1) {
		uint32_t current = mbim_get_frame_current_fragment(frame);
		// need to reconstruct the message
		if(current==0) {
			discard_current_frame(tp);
			tp->current = calloc(1, sizeof(mbim_frame_t)); // create frame structure in tp
			tp->current->data = frame;
			return; // need to finish building
		} else {
			mbim_frame_t *f=tp->current;
			for(uint32_t i=0;f && i<current-1;i++)
				f = f->next;
			if(!f || f->next) { // frame out of order or duplicated, discard it
				discard_current_frame(tp); // discard eventual partial answers already received
				tp->current = calloc(1, sizeof(mbim_frame_t)); // create frame structure in tp
				tp->current->data = frame;
				discard_current_frame(tp); // discard this frame
				return;
			}
			f->next = calloc(1, sizeof(mbim_frame_t)); // append to frame structure in tp
			f->next->data = frame;
		}
		if(current<fragments-1)
			return; // need to finish building
		// build message. then process below
		msg = calloc(1, sizeof(mbim_function_message_t));
		msg->type = mbim_get_frame_msg_type(frame);
		msg->sequence_id = mbim_get_frame_sequence_id(frame);
		mbim_frame_t *f=tp->current;
		for(uint32_t i=0;i<fragments;i++) {
			uint32_t data_len = mbim_get_frame_length(f->data);
			msg->size += data_len-header_len;
			f=f->next;
		}
		msg->bin_buf = malloc(msg->size);
		msg->size=0;
		f=tp->current;
		for(uint32_t i=0;i<fragments;i++) {
			uint32_t data_len = mbim_get_frame_length(f->data);
			memcpy(msg->bin_buf+msg->size, f->data+header_len, data_len-header_len);
			msg->size += data_len-header_len;
			f=f->next;
		}
		mbim_free_frame(tp->current); // delete and set to null frame structure in tp.
		tp->current=NULL;
	} else {
		discard_current_frame(tp); // check if frame structure in tp==null. if not, delete it (limitation: do not produce an error for the function, but if sequence_id>0, unlock the thread with error)
		// build message, then process below
		msg = calloc(1, sizeof(mbim_function_message_t));
		msg->type = mbim_get_frame_msg_type(frame);
		msg->sequence_id = mbim_get_frame_sequence_id(frame);
		if(msg->type==MBIM_OPEN_DONE || msg->type==MBIM_CLOSE_DONE || msg->type==MBIM_FUNCTION_ERROR_MSG)
			header_len = sizeof(uint32_t)*3; // no fragmentation header for these messages
		msg->size=mbim_get_frame_length(frame)-header_len;
		msg->bin_buf = malloc(msg->size);
		memcpy(msg->bin_buf, frame+header_len, msg->size);
		free(frame);
	}

	if(msg->sequence_id>0) { // look for a possible waiting thread
		pthread_mutex_lock(&tp->sq.lock); {
			queue_elem_t* p = tp->sq.head;
			while(p && p->elem) {
				client_params_t *cp = p->elem;
				if(cp->status == COMMAND_STATE_WAIT_ANSWER && cp->sequence_id==msg->sequence_id) {
					cp->response = msg;
					msg = NULL;
					// remove command from the list, unlock condition
					pthread_mutex_unlock((&tp->sq.lock));
					remove_elem_from_queue(&tp->sq, cp);
					pthread_mutex_lock((&tp->sq.lock));
					pthread_cond_signal(&cp->waitcond);
					break;
				}
				p=p->next;
			}
		}
		pthread_mutex_unlock(&tp->sq.lock);

	} else if(msg->type == MBIM_INDICATE_STATUS_MSG) { // look for a possible handler
		int cmd_code = mbim_get_msg_cmd_code(msg);
		pthread_mutex_lock(&tp->eq.lock); { // to prevent insertions and removal at this time
			queue_elem_t* p = tp->eq.head;
			while(p && p->elem) {
				event_handler_t *eh = p->elem;
				if(eh->cmd_code ==  cmd_code) {
					char name[64];
					sprintf(name, "%s-%08X", eh->handler_name, msg->sequence_id);
					client_params_t *cp = new_client_thread(name, tp);
					// need to copy because there can be several handlers
					mbim_function_message_t *m = calloc(1, sizeof(mbim_function_message_t));
					m->bin_buf = malloc(msg->size);
					memcpy(m->bin_buf, msg->bin_buf, msg->size);
					m->type = msg->type;
					m->size = msg->size;
					m->sequence_id = msg->sequence_id; // even if it is 0
					cp->response = m;
					DBGT("spawn: %s", cp->name)
					pthread_create(&cp->tid, NULL, eh->thread_start_function, cp);
				}
				p=p->next;
			}
		}
		pthread_mutex_unlock(&tp->eq.lock);
		mbim_free_function_message(msg); // if unprocessed, delete (printed above)
		msg=NULL;
	}
	if(msg) {
		DBGT("no handler for this frame. Delete.")
		mbim_free_function_message(msg); // if unprocessed, delete (printed above)
	}
}

size_t mbim_process_input(thread_params_t *tp, const unsigned char *buf, size_t size) {
	size_t curproc = 0;
	while(size>=12) { // bare minimum frame size for open and close done
		uint32_t frame_length = mbim_get_frame_length(buf);
		if(size<frame_length)
			return curproc;
		unsigned char* frame = malloc(frame_length);
		memcpy(frame, buf, frame_length);
		process_mbim_frame(tp, frame);
		curproc+=frame_length;
		buf+=frame_length;
		size-=frame_length;
	}
	return curproc;
}

const char waitspinner[] = "-/|\\";

int mbim_process_idle(thread_params_t *tp) {
	mbim_frame_t *frame = NULL;
	pthread_mutex_lock(&tp->sq.lock); {
		queue_elem_t* p = tp->sq.head;
		while(p && p->elem) {
			client_params_t *cp = p->elem;
			if(cp->status == COMMAND_STATE_WAIT_TO_SEND) {
				struct pollfd fds[1];
				int pollret;
				fds[0].fd = tp->fd;
				fds[0].events = POLLOUT | POLLERR | POLLHUP | POLLNVAL;
				frame = mbim_message_to_frames(cp->command, ++tp->mbim_sequence, tp->mbim_MaxControlTransfer);
				cp->sequence_id=tp->mbim_sequence;
				mbim_frame_t *f=frame;
				int framenum=0;
				while(f) {
					print_mbim_frame(f->data);
					size_t bufsize = mbim_get_frame_length(f->data);
					size_t written = 0;
					while(written<bufsize) {
						pollret = poll(fds, 1, tp->timeout_msec);
						if(pollret>0) {
							if(fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
								goto finished; // do not process error here, the reading poll will do it
							}
							written += write(tp->fd, f->data+written, bufsize-written);
						} else if(pollret<0) {
							goto finished;
						}
						// else in case of timeout, just repeat
					}
					cp->status = COMMAND_STATE_WAIT_ANSWER;
					f=f->next;
					framenum++;
				}
				goto finished; // one command sending for cycle
			}
			p=p->next;
		}
	}
finished:
	if(frame)
		mbim_free_frame(frame);
	pthread_mutex_unlock(&tp->sq.lock);
	return IDLE_FINISHED_PROC;
}

void mbim_thread_created(thread_params_t *tp) { // this function should be passed to create_mbim_thread
	// register event handlers
	event_handler_t* eh = calloc(1, sizeof(event_handler_t));
	sprintf(eh->handler_name, "CONNECT");
	eh->cmd_code = MBIM_CID_CONNECT;
	eh->thread_start_function = mbim_event_connect;
	add_event_handler(tp, eh);
}

thread_params_t *create_mbim_thread(const char *portname, uint32_t mbim_MaxControlTransfer) {
	thread_params_t *tp = (thread_params_t *)calloc(1, sizeof(thread_params_t));
	strncpy(tp->name, portname, sizeof(tp->name));
	tp->fd = openport(portname, &tp->oldt, &tp->newt);
	if(tp->fd<0)
		goto error;
	tp->timeout_msec = 10; // acceptable interval for mbim
	tp->thread_created_notify = mbim_thread_created;
	tp->thread_exiting_notify = loop_thread_exiting;
	tp->thread_process_input = mbim_process_input;
	tp->thread_process_idle = mbim_process_idle;
	tp->mbim_MaxControlTransfer = mbim_MaxControlTransfer;
	if(create_loop_thread(tp) != 0)
		goto error;
	return tp;
error:
	DBG(REDCOLOR"no mbim thread! make sure to be sudo"NOCOLOR);
	loop_thread_exiting(tp);
	free(tp);
	return NULL;
}
