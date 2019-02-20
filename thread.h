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

#ifndef __THREAD_H__
#define __THREAD_H__

#include "common.h"
#include "mbim_lib.h"
#include <termios.h>
#include <stdint.h>
#include <pthread.h>

#define THREAD_RECEIVE_BUFSIZE		(64*1024)

enum command_types {
	COMMAND_TYPE_ADMIN	= 1,
	COMMAND_TYPE_PROCEDURE	= 2,
};

enum command_states {
	COMMAND_STATE_WAIT_TO_SEND	= 1,
	COMMAND_STATE_WAIT_ANSWER	= 2,
};

enum commands {
	COMMAND_TERMINATE_THREAD	= 1,
};

enum idle_returns {
	IDLE_CONTINUE_PROC	= 0,
	IDLE_FINISHED_PROC	= 1,
	IDLE_TERMINATE		= -1,
};

enum thread_types {
	THREAD_LOOP		= 100,
	THREAD_UDEV		= 110,
	THREAD_TTY		= 120,
	THREAD_AT		= 131,
	THREAD_MBIM		= 132,
	THREAD_PROCEDURE	= 200,
	THREAD_EVENT		= 300, // almost the same as procedure, expect for its source object
};

// this code uses C11 anonymous struct-in-struct, available with -fplan9-extensions in gcc. If not available, copy struct members
typedef struct {
	char name[64];
	pthread_t tid;
	int thread_type; // loop or subclass, procedure or subclass
} thread_t;

typedef struct thread_params_t thread_params_t;
struct thread_params_t {
	thread_t;
	long timeout_msec;
	int notifyonly;
	int fd;
	int retval;
	struct termios oldt;
	struct termios newt;
	void (*thread_created_notify)(thread_params_t *tp);
	void (*thread_exiting_notify)(thread_params_t *tp);
	size_t (*thread_process_input)(thread_params_t *tp, const unsigned char *buf, size_t size); // return processed bytes that can be removed from the buffer
	int (*thread_process_idle)(thread_params_t *tp); // returns 0 if processing shall continue, otherwise special result
	queue_t sq; // sending queue --> cmdq (command queue)  (for the thread and for sending)

// add event_thread tp->queue, from another thread

	queue_t eq; // event_handler queue -> to be moved to the event_loop thread associated with the port loop_thread
	void (*thread_event_handler)(thread_params_t *tp, void *event);

// replace with default_modem for tty, under which there will be the interfaces
	thread_params_t *tp_at;
	thread_params_t *tp_mbim;

// in thread_data_mbim_t
	uint32_t mbim_sequence;
	uint32_t mbim_MaxControlTransfer;
	mbim_frame_t *current; // for concatenation

// rename to td (thread_data)
	void *ext;
	//void *td;
};

struct modem_t; // temp, not to break the build
typedef struct {
	struct modem_t *current_modem; // to be typed
} thread_tty_data_t;

typedef struct {
	uint32_t mbim_sequence;
	uint32_t mbim_MaxControlTransfer;
	mbim_frame_t *current; // for concatenation
} thread_mbim_data_t;

/*
typedef struct {
	struct udev *udev_ctx;
	struct udev_monitor *udev_mon;
	queue_t modem_list;
} thread_udev_data_t;
*/


/*
	thread_command_t / thread_procedure_t --> maybe a union at this level?
		command type
		1. direct command to the thread, needs
			- an integer for the command to run (terminate_thread)
			- additional information (string, another integer, ...)
			- may use a mutex to block the caller
			- response
			- status for execution scheduling
			> thread_created notification here?
		2. procedure, and port command exchange object, to be sent to the port when possible. requires:
			- external interface scheduling the
			- port details
			- mutex for blocking the associated procedural thread
			- port specific data
			- status for execution scheduling
			- response
*/

typedef struct {
	int cmd_type;
	union {
		int cmd;
		void *command; // uint32_t sequence_id; should be part of this
	};
	int status; // command_states enum
	void *response; // uint32_t sequence_id; should be part of this
} command_param_t;

typedef struct {
	command_param_t;
	thread_t;
	thread_params_t *tp_caller;
	pthread_cond_t waitcond;
	pthread_mutex_t waitmutex;
	thread_params_t *tp_port;
} procedure_params_t;

typedef struct {
	thread_t;
	int cmd_type;
	union {
		int command_adm;
		void *command; // uint32_t sequence_id; should be part of this
	};
	int status; // command_states enum
	void *response;
	uint32_t sequence_id; // for mbim
	thread_params_t *tp_interface; // tp_port would be better
	pthread_cond_t waitcond;
	pthread_mutex_t waitmutex;
} client_params_t;

typedef struct {
	char handler_name[32]; // thread name will be handler_name+msg specific (sequence_id for mbim)
	int cmd_code; // urc command type, for mbim
	queue_elem_t *cmd_prefix; // urc prefix list for AT
	void *(*thread_start_function)(void *thread_data);
} event_handler_t;

// LOOP thread
int create_loop_thread(thread_params_t *tp);
void loop_thread_created(thread_params_t *tp);
void loop_thread_exiting(thread_params_t *tp);

int create_thread(thread_params_t *tp);

// CLIENT thread -> COMMAND thread
client_params_t *new_client_thread(const char *name, thread_params_t *interface);
void send_command(void *cmd, client_params_t *cp);
void destroy_client_thread(client_params_t *cp);

void add_event_handler(thread_params_t *tp_interface, event_handler_t* eh);
void remove_event_handler(thread_params_t *tp_interface, event_handler_t* eh);

// TODO: create global thread registry

#endif /* __THREAD_H__*/
