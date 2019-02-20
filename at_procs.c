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

#include "at_procs.h"
#include <string.h>
#include <stdlib.h>

void *generic_at_client_thread(void *data) {
	client_params_t *cp = data;
	const unsigned char *response;
	pthread_cond_init(&cp->waitcond, NULL); // initialize waiting condition
	pthread_mutex_init(&cp->waitmutex, NULL);
	pthread_mutex_lock(&cp->waitmutex);
	append_elem_to_queue(&cp->tp_interface->sq, cp);
	pthread_cond_wait(&cp->waitcond, &cp->waitmutex);
	// option1 for timeout: use pthread_cond_timedwait
	// option2 for timeout, better: implement it on the loop_thread
	free(cp->command); cp->command=NULL;
	response = cp->response;
	DBGC("COMMAND RESPONSE:'%s'", response);
	free(cp->response); cp->response=NULL;

	pthread_cond_destroy(&cp->waitcond);
	pthread_mutex_destroy(&cp->waitmutex);
	if(cp->command)
		free(cp->command);
	if(cp->response)
		free(cp->response);
	destroy_client_thread(cp);
	return NULL;
}

// return 0=success
int generic_at_send(thread_params_t *tp, const unsigned char *command) {
	char name[64];
	int n = snprintf(name, sizeof(name), "%p.%s", tp, command);
	if(name[n-1]=='\r') name[n-1]=0;
	client_params_t *cp = new_client_thread(name, tp->tp_at);
	char *cmd;
	cmd = strdup(command);
	cmd[strlen(cmd)-1]='\r'; // \n -> \r for at commands // TODO: verify that it is still so...
	cp->command = cmd;
	cp->status = COMMAND_STATE_WAIT_TO_SEND;
	DBGT("spawn: %s", cp->name)
	return pthread_create(&cp->tid, NULL, generic_at_client_thread, cp);
}
