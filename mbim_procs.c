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

#include "mbim_procs.h"
#include "mbim_lib.h"
#include <stdlib.h>
#include <string.h>

void *mbim_initproc_client_thread(void *data) {
	client_params_t *cp = data;

	mbim_message_t *cmd;

	// MBIM OPEN
	cmd = mbim_format_open();
	send_command(cmd, cp);
	mbim_free_message(cmd);

	mbim_function_message_t *msg = cp->response;
	uint32_t ret = bin_to_uint32(msg->bin_buf, USE_LITTLE_ENDIAN);

	mbim_free_function_message(cp->response); // after having consumed the response

	DBGC("mbim_open_done with %u", ret)

	if(ret!=MBIM_STATUS_SUCCESS)
		goto end;

	DBGC()

	// MBIM QUERY DEVICE CAPS
	cmd = mbim_format_query_device_capabilities();
	send_command(cmd, cp);
	mbim_free_message(cmd);

	// TODO: extract and check response, extract maxContexts
	mbim_free_function_message(cp->response); // after having consumed the response

	if(0) // replace with an error in response. this shouldn't fail anyway...
		// inform caller of insuccess
		goto end;

	// SET MBIM DEVICE SUBSCRIBE LIST
	DBGC()
	cmd = mbim_format_set_all_subscriptions();
	send_command(cmd, cp);
	mbim_free_message(cmd);

	// TODO: extract and check response
	mbim_free_function_message(cp->response); // after having consumed the response

	if(0) // replace with an error in response. this shouldn't fail anyway...
		// inform caller of insuccess
		goto end;

	// inform caller of success
	DBGC("init done.")

end:
	destroy_client_thread(cp);
	return NULL;
}

// return 0=success
int mbim_initproc(thread_params_t *tp) {
	client_params_t *cp = new_client_thread("mbim_initproc", tp->tp_mbim);
	DBGT("spawn: %s", cp->name)
	return pthread_create(&cp->tid, NULL, mbim_initproc_client_thread, cp);
}

void *mbim_closeproc_client_thread(void *data) {
	client_params_t *cp = data;

	mbim_message_t *cmd;

	// SET MBIM DEVICE SUBSCRIBE LIST -> RESET
	DBGC()
	cmd = mbim_format_set_subscriptions(0);
	send_command(cmd, cp);
	mbim_free_message(cmd);

	// TODO: extract and check response
	mbim_free_function_message(cp->response); // after having consumed the response

	// MBIM CLOSE
	cmd = mbim_format_close();
	send_command(cmd, cp);
	mbim_free_message(cmd);

	mbim_function_message_t *msg = cp->response;
	uint32_t ret = bin_to_uint32(msg->bin_buf, USE_LITTLE_ENDIAN);

	mbim_free_function_message(cp->response); // after having consumed the response

	DBGC("mbim_close_done with %u", ret)

	destroy_client_thread(cp);
	return NULL;
}

// return 0=success
int mbim_closeproc(thread_params_t *tp) {
	client_params_t *cp = new_client_thread("mbim_closeproc", tp->tp_mbim);
	DBGT("spawn: %s", cp->name)
	return pthread_create(&cp->tid, NULL, mbim_closeproc_client_thread, cp);
}

typedef struct {
	uint32_t connect;
	char apn[101];
} connect_op_t;

void* mbim_connect_client_thread(void *data) {
	client_params_t *cp = data;
	connect_op_t *op = cp->command;
	cp->command=NULL;

	DBG("%u, %s", op->connect, op->apn)

	mbim_message_t *cmd = mbim_format_suscriber_ready_status();
	send_command(cmd, cp);
	mbim_free_message(cmd);
	mbim_free_function_message(cp->response); // normally after having consumed the response

	cmd = mbim_format_set_connect(
		0, // session_id hardcoded here
		op->connect,
		op->apn,
		0, // no authentication
		NULL,
		NULL,
		0, // no compression
		1, // IPv4
		MBIMContextTypeInternet);
/*
	cmd = mbim_format_set_connect(
		0, // session_id hardcoded here
		op->connect,
		op->apn,
		2, // chap
		"vf",
		"vf",
		0, // no compression
		1, // IPv4
		MBIMContextTypeInternet);
*/
	send_command(cmd, cp);
	mbim_free_message(cmd);

	mbim_function_message_t* msg = cp->response;
	uint32_t res = bin_to_uint32(msg->bin_buf+UUID_LEN+sizeof(uint32_t), USE_LITTLE_ENDIAN);
	mbim_free_function_message(cp->response); // after having consumed the response

	DBGC("CONNECT result=%u", res); // 0==MBIM_STATUS_SUCCESS

	if(res!=MBIM_STATUS_SUCCESS || op->connect==0)
		goto end;

	cmd = mbim_format_query_ip_configuration(0);  // session_id hardcoded here
	send_command(cmd, cp);
	mbim_free_message(cmd);

	msg = cp->response;
	// extract IP parameters
	mbim_free_function_message(cp->response); // after having consumed the response

end:
	free(op);
	destroy_client_thread(cp);
	return NULL;
}

// return 0=success
int mbim_connect(thread_params_t *tp, uint32_t connect, const char *apn) {
	client_params_t *cp = new_client_thread("mbim_connect", tp->tp_mbim);
	DBGT("spawn: %s", cp->name)
	connect_op_t *op = calloc(1, sizeof(connect_op_t));
	op->connect = connect;
	strncpy(op->apn, apn, sizeof(op->apn));
	cp->command = op;
	return pthread_create(&cp->tid, NULL, mbim_connect_client_thread, cp);
}

void *mbim_event_connect(void *data) {
	client_params_t *cp = data;
	mbim_function_message_t* msg = cp->response; // msg->bin_buf = {uuid, cid, info_buf_len, info_buf={session_id, activation_state, ...}}

	uint32_t session_id = bin_to_uint32(msg->bin_buf+UUID_LEN+sizeof(uint32_t)*2, USE_LITTLE_ENDIAN);
	uint32_t activation_state = bin_to_uint32(msg->bin_buf+UUID_LEN+sizeof(uint32_t)*3, USE_LITTLE_ENDIAN);

	mbim_free_function_message(msg);

	DBG(REDCOLOR"context: %d - state: %s"NOCOLOR, session_id, get_activation_state_string(activation_state));

	destroy_client_thread(cp);
	return NULL;
}
