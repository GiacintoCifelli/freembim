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

#include "thread_tty.h"
#include "at_procs.h"
#include "mbim_procs.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

void tty_thread_created(thread_params_t *tp) {
	DBGT()
	printf(REDCOLOR"\ttype 'help' for the list of options"NOCOLOR"\n");
}

void discard() {
	DBG("discarded: no device present.")
}

size_t tty_process_input(thread_params_t *tp, const unsigned char *buf, size_t size) {
	// here we have the default stdout behavior, so it will buffer the entire line and return it
	// arrows will generate escape sequences, but backspace removes from the buffer before sending here.
	// the final \n is also included
	if(memcmp(buf,"at",2)==0) {
		// spawn thread and add the line to the at queue
		if(tp->tp_at)
			generic_at_send(tp, buf); else discard();
	} else {
		char command[1024];
		sscanf(buf, "%s", command);
		if(strcmp(command,"help")==0) {
			printf("commands:\n"
				"\thelp\n"
				"\texit\n"
				"\tinit // initialize mbim\n"
				"\tclose // close mbim\n"
				"\tconnect apn // activate apn\n"
				"\tdisconnect apn // deactivate apn\n"
				"\tat... // sends at command\n"
			);
		} else if(strcmp(command,"exit")==0) {
			// to do: send kill signal to all threads and return for the pthread_join in the main
			// but before need to add all threads in a list for that
			DBGT("exiting...\n");
			exit(1);
		} else if(strcmp(command,"init")==0) {
			if(tp->tp_mbim) mbim_initproc(tp); else discard();
		} else if(strcmp(command,"close")==0) {
			if(tp->tp_mbim) mbim_closeproc(tp); else discard();
		} else if(strcmp(command,"connect")==0) {
			char apn[1024];
			sscanf(buf, "%s %s", command, apn);
			if(tp->tp_mbim)  mbim_connect(tp, 1, apn); else discard();
		} else if(strcmp(command,"disconnect")==0) {
			char apn[1024];
			sscanf(buf, "%s %s", command, apn);
			if(tp->tp_mbim) mbim_connect(tp, 0, apn); else discard();
		} else
			printf("unknown command '%s'\n", command);
	}
	return size; // entire buffer processed
}

//thread_params_t *create_tty_thread(thread_params_t *tp_at, thread_params_t *tp_mbim) {
thread_params_t *create_tty_thread() {
	thread_params_t *tp = (thread_params_t *)calloc(1, sizeof(thread_params_t));
	strncpy(tp->name, "stdin", sizeof(tp->name));
	tp->fd = STDIN_FILENO;
	tp->timeout_msec = 10; // 0 is not an option: will have 1 cpu at 100%
	tp->thread_created_notify = tty_thread_created;
	tp->thread_exiting_notify = loop_thread_exiting;
	tp->thread_process_input = tty_process_input;
	tp->thread_process_idle = NULL; // no specific idle processing for this thread
	if(create_loop_thread(tp) != 0)
		goto error;
	return tp;
error:
	loop_thread_exiting(tp);
	free(tp);
	return NULL;
}
