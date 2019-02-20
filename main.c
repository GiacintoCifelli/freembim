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

#include "freembim.h"
#include "common.h"
#include "version.h"
#include "thread.h"
#include "thread_tty.h"
#include "thread_at.h"
#include "thread_mbim.h"
#include "thread_udev.h"
#include <stdlib.h>
#include <unistd.h>

void test_queues() {
	queue_t msg_queue = {NULL, 0};
	DBG()

	pthread_mutex_init(&msg_queue.lock, NULL);

	DBG("%p", msg_queue.head); // nil
	append_elem_to_queue(&msg_queue, (void*)1);
	DBG("%p", msg_queue.head); // v1
	append_elem_to_queue(&msg_queue, (void*)2);
	DBG("%p", msg_queue.head); // v1
	append_elem_to_queue(&msg_queue, (void*)3);
	DBG("%p", msg_queue.head); // v1
	remove_elem_from_queue(&msg_queue, (void*)1);
	DBG("%p", msg_queue.head); // v2
	remove_elem_from_queue(&msg_queue, (void*)3);
	DBG("%p", msg_queue.head); // v2
	remove_elem_from_queue(&msg_queue, (void*)2);
	DBG("%p", msg_queue.head); // nil

	DBG("%p", msg_queue.head); // nil
	append_elem_to_queue(&msg_queue, (void*)1);
	DBG("%p", msg_queue.head); // v1
	append_elem_to_queue(&msg_queue, (void*)2);
	DBG("%p", msg_queue.head); // v1
	append_elem_to_queue(&msg_queue, (void*)3);
	DBG("%p", msg_queue.head); // v1
	remove_elem_from_queue(&msg_queue, (void*)1);
	DBG("%p", msg_queue.head); // v2
	remove_elem_from_queue(&msg_queue, (void*)2);
	DBG("%p", msg_queue.head); // v3
	remove_elem_from_queue(&msg_queue, (void*)3);
	DBG("%p", msg_queue.head); // nil

	pthread_mutex_destroy(&msg_queue.lock);
}

thread_params_t *tp_tty; // temp hack -> add external interfaces dictionary: interfaces.h/c

void testthreads() {
	thread_params_t *tp_udev;
	DBG()
	tp_tty = create_tty_thread();
	sleep(1); // temp hack. have a condition signaled by created();
	tp_udev = create_udev_thread();
	pthread_join(tp_tty->tid, NULL);
	free(tp_tty);
	pthread_join(tp_udev->tid, NULL);
	free(tp_udev);
}

int main(int argc, char *argv[]) {
	DBG("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
	//test_queues();
	testthreads();
}

/* TODO:
 x create MBIM and AT threads when the respective devices are detected
 / add thread signal to terminate it ->
 x add termination call in reception loop
 . rename thread_params_t.ext in .td (thread_data_t)
 . split thread_params_t and client_params_t using td (done? for udev)
 . MBIM: read descriptors
 . tty instance: setModem, defaultModem
 . tty: add idle commands
 . notify tty of new modem/port:
 	create external_interfaces dictionary
 	use idle of tty and specific notify/event function to all interfaces
 . notify tty of removed modem
 / use xgets/xprintf: no, interferes with loop_thread
 . at (& maybe tty): split input in lines, process one-by-one
 . destroy threads when device disconnected
 . destroy threads on exit
 . add max_outstanding in mbim
*/
