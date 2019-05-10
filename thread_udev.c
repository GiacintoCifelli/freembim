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

queue_elem_t *find_modem(const char *syspath, queue_t *modem_list) {
	pthread_mutex_lock(&modem_list->lock);
	queue_elem_t *p = modem_list->head;
	while(p) {
		modem_t * m = p->elem;
		if(strcmp(m->syspath,syspath)==0)
			goto end;
		p = p->next;
	}
end:
	pthread_mutex_unlock(&modem_list->lock);
	return p;
}

modem_t *get_modem(const char *syspath, const char *vendor, const char *model, const char *devname, const char *handler, queue_t *modem_list) {
	queue_elem_t *p = find_modem(syspath, modem_list);
	if(p) return p->elem;

	modem_t *m = calloc(1, sizeof(modem_t));
	if(syspath) m->syspath = strdup(syspath);
	if(devname) m->devname = strdup(devname);
	if(vendor) m->vendor = strdup(vendor);
	if(model) m->model = strdup(model);
	if(handler) m->handler = strdup(handler);
	m->interfaces = create_queue();
	m->usb_ports = create_queue();

	append_elem_to_queue(modem_list, m);
	return m;
}

void add_interface(modem_t *modem, const char *number, const char *subsystem, const char *devnode) {
	interface_t *i = calloc(1, sizeof(interface_t));
	i->number = strdup(number);
	i->subsystem = strdup(subsystem);
	i->devnode = strdup(devnode);
	append_elem_to_queue(modem->interfaces, i);
}

void remove_modem(const char *syspath, queue_t *modem_list) {
	queue_elem_t *p = find_modem(syspath, modem_list);
	if(!p) return;
	modem_t *m = p->elem;
	remove_elem_from_queue(modem_list, m);
	while(m->usb_ports->head) {
		free(m->usb_ports->head->elem); // placeholder
		remove_elem_from_queue(m->usb_ports, m->usb_ports->head->elem);
	}
	free_queue(m->usb_ports); // need to complete, with free of each element
	while(m->interfaces->head) {
		interface_t *i = m->interfaces->head->elem;
		remove_elem_from_queue(m->interfaces, i);
		free(i->devnode);
		free(i->subsystem);
		free(i->number);
		free(i);
	}
	free_queue(m->interfaces); // need completion, with free of each element
	free(m->syspath);
	free(m->devname);
	free(m->vendor);
	free(m->model);
	free(m->handler);
	free(m);
}

static int add_device(const char *syspath, const char *devname, const char *handler, const char *vendor, const char *model, struct udev_device *device, queue_t *modem_list) {
	struct udev_device *usb_interface;
	const char *devpath, *devnode, *interface, *number;
	const char *label, *sysattr=NULL, *subsystem;
	struct udev_device *parent;
	devpath = udev_device_get_syspath(device);
	if(!devpath) return 0;
	devnode = udev_device_get_devnode(device);
	if(!devnode) devnode = udev_device_get_property_value(device, "INTERFACE");
	if(!devnode) return 0;
	usb_interface = udev_device_get_parent_with_subsystem_devtype(device, "usb", "usb_interface");
	if(!usb_interface) return 0;
	modem_t *modem = get_modem(syspath, vendor, model, devname, handler, modem_list);
	interface = udev_device_get_property_value(usb_interface, "INTERFACE");
	number = udev_device_get_property_value(device, "ID_USB_INTERFACE_NUM");
	if(!number) number = udev_device_get_sysattr_value(device, "bInterfaceNumber");
	if(!number) {
		parent = udev_device_get_parent(device);
		number = udev_device_get_sysattr_value(parent, "bInterfaceNumber");
	}
	label = udev_device_get_property_value(device, "OFONO_LABEL");
	if(!label) label = udev_device_get_property_value(usb_interface, "OFONO_LABEL");
	subsystem = udev_device_get_subsystem(device);
	sysattr = udev_device_get_sysattr_value(device, "device/interface");
	add_interface(modem, number, subsystem, devnode);
	return 1;
}

int add_if_modem(struct udev_device *device, queue_t *modem_list) {
	int ret = 0;
	const char *bus;
	const char *ofono_ignore_device;
	bus = udev_device_get_property_value(device, "ID_BUS");
	ofono_ignore_device = udev_device_get_property_value(device, "OFONO_IGNORE_DEVICE");
	if(!bus) bus = udev_device_get_subsystem(device);
	if(!bus || ofono_ignore_device) goto skip;
	if(strcmp(bus, "usb")==0 || strcmp(bus, "usbmisc")==0) {
		struct udev_device *usb_device;
		const char *syspath, *devname, *handler;
		const char *vendor = NULL, *model = NULL;
		usb_device = udev_device_get_parent_with_subsystem_devtype(device, "usb", "usb_device");
		if(!usb_device) goto skip;
		syspath = udev_device_get_syspath(usb_device);
		devname = udev_device_get_devnode(usb_device);
		if(!syspath || !devname) goto skip;
		vendor = udev_device_get_property_value(usb_device, "ID_VENDOR_ID");
		model = udev_device_get_property_value(usb_device, "ID_MODEL_ID");

		handler = udev_device_get_property_value(usb_device, "OFONO_DRIVER");
		if(!handler) {
			struct udev_device *usb_interface = udev_device_get_parent_with_subsystem_devtype(device, "usb", "usb_interface");
			if (usb_interface) handler = udev_device_get_property_value(usb_interface, "OFONO_DRIVER");
		}
		if (!handler) {
			const char *serialdriver = udev_device_get_property_value(device, "OFONO_DRIVER"); /* for serial2usb devices (enum as tty/generic) */
			if (serialdriver) {
				//add_serial_device(device);
			}
		}
		if (!handler) {
			const char *drv;
			unsigned int i;
			drv = udev_device_get_property_value(device, "ID_USB_DRIVER");
			if(!drv) drv = udev_device_get_driver(device);
			if(!drv) {
				struct udev_device *parent;
				parent = udev_device_get_parent(device);
				if(!parent) goto skip;
				drv = udev_device_get_driver(parent);
				if(!drv) goto skip;
			}
			if(!vendor || !model) goto skip;
		}

		return add_device(syspath, devname, handler, vendor, model, device, modem_list);

	} else {
		// add_serial_device(device);
	}
skip:
	udev_device_unref(device);
	return ret;
}

int udev_process_idle(thread_params_t *tp) {
	thread_udev_ext_t *ext = tp->ext;
	tp->thread_process_idle = NULL; // remove specific thread processing

	queue_elem_t *p = ext->modem_list.head;
	while(p) {
		modem_t * m = p->elem;
		queue_elem_t *q = m->interfaces->head;
		while(q) {
			interface_t *i = q->elem;
			if(strcmp(m->vendor,"1e2d")==0 && (strcmp(m->model,"0065")==0 || strcmp(m->model,"005d")==0)) {
				if(strcmp(i->subsystem,"tty")==0) {
					if(strcmp(i->number,"00")==0) {
					//if(strcmp(i->number,"02")==0) {
						DBGT("create AT loop for: %s", i->devnode)
						thread_params_t *tp = create_at_thread(i->devnode);
						append_elem_to_queue(m->usb_ports, tp);
						// notify tty
						tp_tty->tp_at = tp;
					}
				} else if(strcmp(i->subsystem,"usbmisc")==0) {
					DBGT("create MBIM loop for: %s", i->devnode)
					thread_params_t *tp = create_mbim_thread(i->devnode, 4096);
					append_elem_to_queue(m->usb_ports, tp);
					// notify tty
					tp_tty->tp_mbim = tp;
				}
			}
			q = q->next;
		}
		p = p->next;
	}
	return IDLE_FINISHED_PROC;
}

size_t udev_process_input(thread_params_t *tp, const unsigned char *buf, size_t size) {
	thread_udev_ext_t *ext = tp->ext;
	struct udev_device *device;
	const char *action;
	device = udev_monitor_receive_device(ext->udev_mon);
	if (device == NULL)
		goto end;
	action = udev_device_get_action(device);
	if (action == NULL)
		goto end;
	if(strcmp(action,"add")==0) {
		if(add_if_modem(device, &ext->modem_list))
			tp->thread_process_idle = udev_process_idle;
	} else if(strcmp(action, "remove")==0) {
		const char *syspath = udev_device_get_syspath(device);
		remove_modem(syspath, &ext->modem_list);
	}
end:
	return size; // entire buffer processed
}

void udev_thread_exiting(thread_params_t *tp) {
	DBGT()
	thread_udev_ext_t *ext = tp->ext;
	if(ext->udev_mon) {
		udev_monitor_filter_remove(ext->udev_mon);
		udev_monitor_unref(ext->udev_mon);
	}
	if(ext->udev_ctx)
		udev_unref(ext->udev_ctx);
	destroy_queue(&ext->modem_list);
	free(tp->ext);
	free(tp);
}

thread_params_t *create_udev_thread() {
	thread_params_t *tp = (thread_params_t *)calloc(1, sizeof(thread_params_t));
	thread_udev_ext_t *ext = calloc(1, sizeof(thread_udev_ext_t));
	tp->ext = ext;

	ext->udev_ctx = udev_new();
	if(!ext->udev_ctx)
		goto error;
	ext->udev_mon = udev_monitor_new_from_netlink(ext->udev_ctx, "udev");
	if(!ext->udev_mon)
		goto error;
	udev_monitor_filter_add_match_subsystem_devtype(ext->udev_mon, "tty", NULL);
	udev_monitor_filter_add_match_subsystem_devtype(ext->udev_mon, "usb", NULL);
	udev_monitor_filter_add_match_subsystem_devtype(ext->udev_mon, "usbmisc", NULL);
	udev_monitor_filter_add_match_subsystem_devtype(ext->udev_mon, "net", NULL);
	udev_monitor_filter_add_match_subsystem_devtype(ext->udev_mon, "hsi", NULL);
	udev_monitor_filter_update(ext->udev_mon);
	if(udev_monitor_enable_receiving(ext->udev_mon) < 0)
		goto error;
	init_queue(&ext->modem_list);
	struct udev_enumerate *enumerate;
	struct udev_list_entry *entry;
	enumerate = udev_enumerate_new(ext->udev_ctx);
	if (enumerate == NULL)
		goto skip;
	udev_enumerate_add_match_subsystem(enumerate, "tty");
	udev_enumerate_add_match_subsystem(enumerate, "usb");
	udev_enumerate_add_match_subsystem(enumerate, "usbmisc");
	udev_enumerate_add_match_subsystem(enumerate, "net");
	udev_enumerate_add_match_subsystem(enumerate, "hsi");
	udev_enumerate_scan_devices(enumerate);
	entry = udev_enumerate_get_list_entry(enumerate);
	int num_devices = 0;
	while (entry) {
		const char *syspath = udev_list_entry_get_name(entry);
		struct udev_device *device;

		device = udev_device_new_from_syspath(ext->udev_ctx, syspath);
		if(device) {
			num_devices += add_if_modem(device, &ext->modem_list);
			udev_device_unref(device);
		}
		entry = udev_list_entry_get_next(entry);
	}
	udev_enumerate_unref(enumerate);
skip:
	strncpy(tp->name, "udev", sizeof(tp->name));
	tp->fd = udev_monitor_get_fd(ext->udev_mon);
	tp->notifyonly = 1;
	tp->timeout_msec = 1000; // 1 second gives a composite device the time to enumerate all devices
	tp->thread_process_input = udev_process_input;
	if(num_devices) // modems available?
		tp->thread_process_idle = udev_process_idle;
	else
		tp->thread_process_idle = NULL; // no specific idle processing for this thread at startup
	tp->thread_exiting_notify = udev_thread_exiting;
	if(create_loop_thread(tp) != 0)
		goto error;
	return tp;
error:
	udev_thread_exiting(tp);
	free(tp);
	return NULL;
}
