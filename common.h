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

#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>
#include <pthread.h>
#include <termios.h>
#include <stdio.h>

// conversions /////////////////////////////////////////////////////////////////

/* inttypes:
int8_t
int16_t
int32_t
int64_t
uint8_t
uint16_t
uint32_t
uint64_t
*/

/* MBIM standard talks about utf-16, but limited to the BMP range, which effectively makes the sequence UCS2.
It is better assumed that an MBIM string is an UCS2 sequence, because in that case it makes more sense to have each rune in little-endian
*/

typedef struct {
	unsigned int use_bmp_only : 1;
	unsigned int use_little_endian : 1;
} sparam_t;

enum conversion_params {
	USE_BMP_ONLY			= 1<<0, /* BMP: Basic Multilingual Plane */
	USE_LITTLE_ENDIAN		= 2<<0,
};

enum string_conversion_error {
	INCOMPLETE_RUNE			= 1,
	INVALID_RUNE			= 2,
	CHARACTER_OUTSIDE_RANGE		= 3,
	INCOMPLETE_HEX_VALUE		= 4,
};

/* converts an ASCIIZ string UTF8-encoded to an UCS2 string (without trailing 0x0000 at the end)
 * returns the length in runes of the UCS2 string if >=0, otherwise a -string_conversion_error value */
int utf8_to_ucs2(const char *utf8, unsigned char* utf16, int params);
/* >=0 number of runes, <0 error in utf8 format */
int count_runes_utf8(const char *utf8, int params);

int ucs2_to_utf8(const unsigned char *ucs2, int num_runes, char *utf8, int params);
int buflen_ucs2_to_utf8(const unsigned char *ucs2, int num_runes, int params);

// ignore extra spaces and dashes, use uppercase/lowercase, return bin_buf length if >=0, -error if <0
int hex_to_bin(const char *hex_buf, unsigned char*bin_buf);
int hex_to_bin_len(const char *hex_buf);

uint32_t bin_to_uint32(const unsigned char* buf, int params);
uint64_t bin_to_uint64(const unsigned char* buf, int params);

#define FUINT32LE	"%02X%02X%02X%02X "
#define VUINT32BE(a)	(((a)>>24) & 0xFF),(((a)>>16) & 0xFF),(((a)>>8) & 0xFF),((a) & 0xFF)
#define VUINT32LE(a)	((a) & 0xFF),(((a)>>8) & 0xFF),(((a)>>16) & 0xFF),(((a)>>24) & 0xFF)

void print_hexa(const unsigned char *buf, size_t len);

char *strdup_printf(char* format, ...);

// queues //////////////////////////////////////////////////////////////////////

typedef struct queue_elem_t queue_elem_t;
struct queue_elem_t {
	queue_elem_t *next;
	void *elem;
};

typedef struct {
	queue_elem_t *head;
	pthread_mutex_t lock;
} queue_t;

queue_t *create_queue(); // does calloc and init
void init_queue(queue_t *queue); // initialize mutex
void append_elem_to_queue(queue_t *queue, void *elem);
void remove_elem_from_queue(queue_t *queue, void *elem);
void destroy_queue(queue_t *queue); // destroys mutex
void free_queue(queue_t *queue); // destroy and free

void test_queues();

// file management /////////////////////////////////////////////////////////////

int openport(const char *portname, struct termios *oldt, struct termios *newt);
void closeport(int port, struct termios *oldt);

// debug ///////////////////////////////////////////////////////////////////////

#define REDCOLOR "\x1b\x5b\x30\x31\x3b\x33\x31\x6d"
#define NOCOLOR "\x1b\x5b\x30\x30\x6d"

const char *getdatetime();
const char *getdatetimeshort();

void print_trace();

#define DBG(fmt, arg...) printf("%s-%s:%d-%s() " fmt "\n", getdatetimeshort(), __FILE__, __LINE__, __FUNCTION__ , ## arg);

#define DBGT(fmt, arg...) printf("%s-%s-%s:%d-%s() " fmt "\n", getdatetimeshort(), tp->name, __FILE__, __LINE__, __FUNCTION__ , ## arg);
#define DBGC(fmt, arg...) printf("%s-%s-%s:%d-%s() " fmt "\n", getdatetimeshort(), cp->name, __FILE__, __LINE__, __FUNCTION__ , ## arg);

#endif /* __COMMON_H__ */
