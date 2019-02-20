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

#include "common.h"
#include <stdlib.h>
#include <fcntl.h>
#include <asm/ioctls.h>
#include <linux/serial.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdarg.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>

// conversions /////////////////////////////////////////////////////////////////

int utf8_to_ucs2(const char *utf8, unsigned char* ucs2, int params) {
	int n = 0;
	uint32_t rune;
	if(ucs2)
		params |= USE_BMP_ONLY;
	while(*utf8) {
		if(*utf8 & 0x80) {
			uint8_t h = *utf8;
			utf8++;
			rune = 0;
			while(h&0x40) {
				if(!*utf8)
					return -INCOMPLETE_RUNE;
				if((*utf8 & 0xC0) != 0x80)
					return -INVALID_RUNE;
				rune = (rune<<6) | (*utf8 & 0x3F);
				h <<= 1;
				utf8++;
			}
		} else {
			rune = *utf8;
			utf8++;
		}
		if((params & USE_BMP_ONLY) && (rune>0xFFFF || (rune>0xD7FF && rune<0xE000)))
			return -CHARACTER_OUTSIDE_RANGE;
		if(ucs2) {
			if(USE_LITTLE_ENDIAN) {
				*ucs2++ = (rune & 0xFF);
				*ucs2++ = ((rune<<8) & 0xFF);
			} else {
				*ucs2++ = ((rune<<8) & 0xFF);
				*ucs2++ = (rune & 0xFF);
			}
		}
		n++;
	}
	return n;
}

int count_runes_utf8(const char *utf8, int params) {
	return utf8_to_ucs2(utf8, NULL, params);
}

uint32_t bin_to_uint32(const unsigned char* buf, int params) {
	if(params&USE_LITTLE_ENDIAN)
		return (buf[3]<<24)|(buf[2]<<16)|(buf[1]<<8)|buf[0];
	else
		return (buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3];
}

//int ucs2_to_utf8(const unsigned char *ucs2, int num_runes, char *utf8, int params);
//int buflen_ucs2_to_utf8(const unsigned char *ucs2, int num_runes, int params);

unsigned int hex2binC(const char c) {
	if(c>='0' && c<='9')
		return c-'0';
	if(c>='A' && c<='F')
		return c-'A'+0x0a;
	if(c>='a' && c<='f')
		return c-'a'+0x0a;
	return -1;
}

// returns length of bin buffer (also if bin_buf==0)
int hex_to_bin(const char *hex_buf, unsigned char *bin_buf) {
	uint32_t size=0;
	unsigned char *bin_buf0 = bin_buf;
	const char *hex_buf0 = hex_buf;
	while(*hex_buf) {
		char c = hex_buf[0];
		if(hex2binC(c)!=-1) {
			char c2 = hex_buf[1];
			if(hex2binC(c2)!=-1) {
				if(bin_buf) {
					unsigned char v = ((hex2binC(c)<<4) & 0xF0) | (hex2binC(c2) & 0x0F);
					*bin_buf = v;
					bin_buf++;
				}
			} else
				return -INCOMPLETE_HEX_VALUE;
			hex_buf+=2;
			size+=2;
		} else
			hex_buf++;
	}
	return size/2;
}

int hex_to_bin_len(const char *hex_buf) {
	return hex_to_bin(hex_buf, NULL);
}

void print_hexa(const unsigned char *buf, size_t len) {
	uint32_t i;
	printf("\n");
	for(i=0;i<len;i++) {
		uint8_t v = buf[i];
		printf("%02X ", v);
		if((i+1)%4 == 0) printf(" ");
		if((i+1)%8 == 0) printf("  ");
		if((i+1)%16 == 0) printf("\n");
	}
	printf("\n"); fflush(stdout);
}

char *strdup_printf(char* format, ...) {
	va_list args;
  	va_start (args, format);
  	char *s = NULL;
	size_t size = vsnprintf (s, 0, format, args)+1;
	va_end (args);
  	va_start (args, format);
	s = calloc(size,sizeof(char));
	vsnprintf (s, size, format, args);
	va_end (args);
	return s;
}

// queues //////////////////////////////////////////////////////////////////////

queue_t *create_queue() {
	queue_t *queue = calloc(1, sizeof(queue_t));
	init_queue(queue);
	return queue;
}

void init_queue(queue_t *queue) {
	pthread_mutex_init(&queue->lock, NULL);
}

void append_elem_to_queue(queue_t *queue, void *elem) {
	queue_elem_t* newelem = (queue_elem_t*)calloc(1, sizeof(queue_elem_t));
	newelem->elem = elem;

	pthread_mutex_lock(&queue->lock);
		if(!queue->head)
			queue->head = newelem;
		else {
			queue_elem_t* p = queue->head;
			while(p->next)
				p=p->next;
			p->next = newelem;
		}
	pthread_mutex_unlock(&queue->lock);
};

void remove_elem_from_queue(queue_t *queue, void *elem) {
	queue_elem_t* oldelem;

	// assume that elem is in queue, no check.
	pthread_mutex_lock(&queue->lock);
		if(queue->head->elem == elem) {
			oldelem = queue->head;
			queue->head = queue->head->next;
		} else {
			queue_elem_t* p = queue->head;
			while(p->next->elem != elem)
				p=p->next;
			oldelem = p->next;
			p->next = oldelem->next;
		}
		//free(oldelem->elem);
		free(oldelem);
	pthread_mutex_unlock(&queue->lock);
};

void destroy_queue(queue_t *queue) {
	pthread_mutex_destroy(&queue->lock);
}

void free_queue(queue_t *queue) {
	destroy_queue(queue);
	while(queue->head)
		remove_elem_from_queue(queue, queue->head);
	free(queue);
}

// file management /////////////////////////////////////////////////////////////

int openport(const char *portname, struct termios *oldt, struct termios *newt) {
	int port;
	struct serial_struct old, new;
	int DTR_flag = TIOCM_DTR;

	port = open(portname, O_RDWR | O_NONBLOCK);
	if(port>=0) {
		tcgetattr(port, oldt);
		tcgetattr(port, newt);

		newt->c_iflag &= ~IGNBRK;         // disable break processing // disable IGNBRK for mismatched speed tests; otherwise receive break as \000 chars
		newt->c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

		newt->c_oflag = 0;                // no remapping, no delays

		newt->c_cflag |= (CLOCAL | CREAD);               // ignore modem controls. always set these
		newt->c_cflag |= CS8;                            // 8-bit chars      8
		newt->c_cflag &= ~(PARENB | PARODD);             // shut off parity  N
		newt->c_cflag &= ~CSTOPB;                        // 1 stop bit       1

		//newt->c_lflag &= ~(ICANON|ECHO|ECHOE|ECHONL|IEXTEN|ISIG);
		newt->c_lflag = 0;                               // no signaling chars, no echo, no canonical processing

		newt->c_cc[VMIN]  = 0; // nonblocking
		newt->c_cc[VTIME] = 0; // 5 = 0.5 seconds read timeout -> immediately if no further chars are waiting

		tcsetattr(port, TCSANOW, newt);
		tcgetattr(port, newt);

		ioctl(port, TIOCGSERIAL, &old);
		new = old;
		new.closing_wait = ASYNC_CLOSING_WAIT_NONE;
		ioctl(port, TIOCSSERIAL, &new);

		ioctl(port, TIOCMBIS, &DTR_flag);
	}
	return port;
}

void closeport(int port, struct termios *oldt) {
	if(port>=0){
		tcsetattr(port, TCSANOW, oldt);
		close(port);
		port=-1;
	}
}

// debug ///////////////////////////////////////////////////////////////////////

const char *getdatetime()
{
	static char dt[32];
	time_t o=time(NULL);
	strftime(dt,sizeof(dt),"[%F, %T] ",localtime(&o));
	return (const char*)dt;
}

const char *getdatetimeshort()
{
	static char dt[32];
	time_t o=time(NULL);
	strftime(dt,sizeof(dt),"%Y%m%d%H%M%S",localtime(&o));
	return (const char*)dt;
}

void print_trace() {
	char pid_buf[30];
	char name_buf[512];
	int child_pid;
	sprintf(pid_buf, "%d", getpid());
	name_buf[readlink("/proc/self/exe", name_buf, 511)]=0;
	child_pid = fork();
	if (!child_pid) {
		dup2(2,1); // redirect output to stderr
		fprintf(stdout,"stack trace for %s pid=%s\n",name_buf,pid_buf);
		execlp("gdb", "gdb", "--batch", "-n", "-ex", "thread", "-ex", "bt", name_buf, pid_buf, NULL);
		abort(); /* If gdb failed to start */
	} else {
		waitpid(child_pid,NULL,0);
	}
}

