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

////////////////////////////////////////////////////////////////////////////////
/* Terminology
USB standard defines
	a product as a device or hub.
	a device has one or more interfaces, each having one or more endpoints.
	endpoints are not our concern, taken care of by the kernel
UDEV defines any kernel object as a device, in particular:
	an USB interface is a UDEV device (and also the entire composite device)
	a serial port is a a UDEV device
Linux reserves the term interface to a network interface.

we use:
	usb_device or serial_device for the product
	usb_interface for the usb interfaces
	port for the usb_interface devnode (eg: /dev/ttyACM7)

*/
////////////////////////////////////////////////////////////////////////////////

#include "thread_udev.h"
#include "thread_at.h"
#include "thread_mbim.h"
#include <string.h>
#include <stdlib.h>
#include <libudev.h>

typedef struct {
	struct udev *udev_ctx;
	struct udev_monitor *udev_mon;
	queue_t modem_list;
} thread_udev_ext_t;

typedef struct {
	char *syspath; // syspath+"/descriptors" for mbim properties
} udev_object;

typedef struct {
	char *number;
	char *subsystem;
	char *devnode;
} interface_t;

typedef struct {
	udev_object;
	int type; // serial/usb
	queue_t *interfaces;
} device_t;

typedef struct {
	char *vendor;
	char *model;
	char *devname;
	char *handler;

	device_t; // udev composite device ID's
	queue_t *usb_ports; // loop_threads with port path string handling it
} modem_t;

extern thread_params_t *tp_tty; // temp hack

int udev_process_idle(thread_params_t *tp) {
	tp->thread_process_idle = NULL; // remove specific thread processing
	char *atport = "/dev/ttyACM0"; // hardcoded, may need to be changed manually
	DBGT("create AT loop for: %s", atport)
	thread_params_t *tp2 = create_at_thread(atport);
	tp_tty->tp_at = tp2;
	char *mbimport = "/dev/cdc-wdm1"; // hardcoded, may need to be changed manually (for example to cdc-wdm0)
	DBGT("create MBIM loop for: %s", mbimport)
	tp2 = create_mbim_thread(mbimport, 4096);
	tp_tty->tp_mbim = tp2;
	return IDLE_FINISHED_PROC;
}

size_t udev_process_input(thread_params_t *tp, const unsigned char *buf, size_t size) {
	return 0;
}

void udev_thread_exiting(thread_params_t *tp) {
	DBGT()
	thread_udev_ext_t *ext = tp->ext;
	destroy_queue(&ext->modem_list);
	free(tp->ext);
	free(tp);
}

thread_params_t *create_udev_thread() {
	thread_params_t *tp = (thread_params_t *)calloc(1, sizeof(thread_params_t));
	thread_udev_ext_t *ext = calloc(1, sizeof(thread_udev_ext_t));
	tp->ext = ext;

	strncpy(tp->name, "udev", sizeof(tp->name));
	//tp->fd = udev_monitor_get_fd(ext->udev_mon);
	tp->notifyonly = 1;
	tp->timeout_msec = 1000; // 1 second gives a composite device the time to enumerate all devices
	tp->thread_process_input = udev_process_input;
	tp->thread_process_idle = udev_process_idle;
	tp->thread_exiting_notify = udev_thread_exiting;
	if(create_loop_thread(tp) != 0)
		goto error;
	return tp;
error:
	udev_thread_exiting(tp);
	free(tp);
	return NULL;
}
