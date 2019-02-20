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

#include "mbim_lib.h"
#include "common.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

void bin_to_hex(const unsigned char*bin, char *hex, size_t binbuflen) {
	for(size_t i=0;i<binbuflen;i++) {
		unsigned char c = *bin;
		sprintf(hex, "%02X", c);
		bin++;
		hex+=2;
	}
}

// direct conversion utf8->ucs2->hex, with error checking and allocation
char *mbim_get_string(const unsigned char*utf8) {
	int runes;
	if(!utf8) return "";
	runes = count_runes_utf8(utf8, USE_BMP_ONLY);
	if(runes<0) return NULL;
	if(runes==0) return "";
	unsigned char *ucs2_buf = alloca(runes*2);
	utf8_to_ucs2(utf8, ucs2_buf, USE_BMP_ONLY|USE_LITTLE_ENDIAN);
	char *hexbuf = calloc(runes*4+1, sizeof(char));
	bin_to_hex(ucs2_buf, hexbuf, runes*2);
	return hexbuf;
}

void mbim_free_message(mbim_message_t *msg) {
	if(!msg) return;
	if(msg->hex_buf)
		free(msg->hex_buf);
	free(msg);
}

void mbim_free_function_message(mbim_function_message_t *msg) {
	if(!msg) return;
	if(msg->bin_buf)
		free(msg->bin_buf);
	free(msg);
}

mbim_message_t *mbim_format_open() {
	mbim_message_t *msg = calloc(1, sizeof(mbim_message_t));
	msg->type = MBIM_OPEN;
	return msg;
}

mbim_message_t *mbim_format_close() {
	mbim_message_t *msg = calloc(1, sizeof(mbim_message_t));
	msg->type = MBIM_CLOSE;
	return msg;
}

mbim_message_t *mbim_format_query_device_capabilities() {
	mbim_message_t *msg = calloc(1, sizeof(mbim_message_t));

	uint32_t code = mbim_get_cmd_code(MBIM_CID_DEVICE_CAPS);
	UUID_t uuid = mbim_get_uuid(MBIM_CID_DEVICE_CAPS);
	uint32_t command_type = 0; // query

	msg->type = MBIM_COMMAND_MSG;
	msg->hex_buf = strdup_printf("%s " FUINT32LE FUINT32LE FUINT32LE, uuid, VUINT32LE(code), VUINT32LE(command_type), VUINT32LE(0));

	return msg;
}

mbim_message_t *mbim_format_set_subscriptions(int ElementCount, ...) { // paramlist TODO
	mbim_message_t *msg = calloc(1, sizeof(mbim_message_t));
	uint32_t code = mbim_get_cmd_code(MBIM_CID_DEVICE_SERVICE_SUBSCRIBE_LIST);
	UUID_t uuid = mbim_get_uuid(MBIM_CID_DEVICE_SERVICE_SUBSCRIBE_LIST);
	uint32_t command_type = 1; // set

	char *buffer = NULL;
	size_t size = snprintf(buffer, 0, FUINT32LE, VUINT32LE(0))*(1+2*ElementCount);

	va_list args;
  	va_start (args, ElementCount);
	for(int i = 0;i<ElementCount;i++) {
		char *subGroup = va_arg(args, char*);
		size += strlen(subGroup);
	}
	va_end (args);
	size++;
	buffer = calloc(size, sizeof(char));
	int pos = snprintf(buffer, size, FUINT32LE, VUINT32LE(ElementCount));
	uint32_t offset = 4+8*ElementCount;
  	va_start (args, ElementCount);
	for(int i = 0;i<ElementCount;i++) {
		char *subGroup = va_arg(args, char*);
		uint32_t grouplen = hex_to_bin_len(subGroup);
		pos += snprintf(buffer+pos, size-pos, FUINT32LE FUINT32LE, VUINT32LE(offset), VUINT32LE(grouplen));
		offset += grouplen;
	}
	va_end (args);
  	va_start (args, ElementCount);
	for(int i = 0;i<ElementCount;i++) {
		char *subGroup = va_arg(args, char*);
		pos += snprintf(buffer+pos, size-pos, "%s", subGroup);
	}
	va_end (args);

	msg->type = MBIM_COMMAND_MSG;
	msg->hex_buf = strdup_printf("%s " FUINT32LE FUINT32LE FUINT32LE "%s", uuid, VUINT32LE(code), VUINT32LE(command_type), VUINT32LE(hex_to_bin_len(buffer)), buffer);
	free(buffer);
	return msg;
}

char *mbim_get_subscription_group(UUID_t uuid, uint32_t CidCount, ...) {
	char *s = NULL;
	size_t size = snprintf (s, 0, "%s " FUINT32LE, uuid, VUINT32LE(CidCount)) + snprintf(s, 0, FUINT32LE, VUINT32LE(0))*CidCount+1;
	s = calloc(size, sizeof(char));
	int pos = snprintf(s, size, "%s " FUINT32LE, uuid, VUINT32LE(CidCount));
	va_list args;
  	va_start (args, CidCount);
	for(int i = 0;i<CidCount;i++) {
		uint32_t v = va_arg(args, uint32_t);
		pos += snprintf(s+pos, size-pos, FUINT32LE, VUINT32LE(v));
	}
	va_end (args);
	return s;
}

mbim_message_t *mbim_format_set_all_subscriptions() {
	char *groupBASIC_CONNECT = mbim_get_subscription_group(UUID_BASIC_CONNECT, 11,
		mbim_get_cmd_code(MBIM_CID_SUBSCRIBER_READY_STATUS),
		mbim_get_cmd_code(MBIM_CID_RADIO_STATE),
		mbim_get_cmd_code(MBIM_CID_PREFERRED_PROVIDERS),
		mbim_get_cmd_code(MBIM_CID_REGISTER_STATE),
		mbim_get_cmd_code(MBIM_CID_PACKET_SERVICE),
		mbim_get_cmd_code(MBIM_CID_SIGNAL_STATE),
		mbim_get_cmd_code(MBIM_CID_CONNECT),
		mbim_get_cmd_code(MBIM_CID_PROVISIONED_CONTEXTS),
		mbim_get_cmd_code(MBIM_CID_IP_CONFIGURATION),
		mbim_get_cmd_code(MBIM_CID_EMERGENCY_MODE),
		mbim_get_cmd_code(MBIM_CID_MULTICARRIER_PROVIDERS)
	);
	char *groupSMS = mbim_get_subscription_group(UUID_SMS, 3,
		mbim_get_cmd_code(MBIM_CID_SMS_CONFIGURATION),
		mbim_get_cmd_code(MBIM_CID_SMS_READ),
		mbim_get_cmd_code(MBIM_CID_SMS_MESSAGE_STORE_STATUS)
	);
	char *groupUSSD = mbim_get_subscription_group(UUID_USSD, 1,
		mbim_get_cmd_code(MBIM_CID_USSD)
	);
	char *groupPHONEBOOK = mbim_get_subscription_group(UUID_PHONEBOOK, 1,
		mbim_get_cmd_code(MBIM_CID_PHONEBOOK_CONFIGURATION)
	);
	char *groupSTK = mbim_get_subscription_group(UUID_STK, 1,
		mbim_get_cmd_code(MBIM_CID_STK_PAC)
	);
	mbim_message_t *ret = mbim_format_set_subscriptions(5, groupBASIC_CONNECT, groupSMS, groupUSSD, groupPHONEBOOK, groupSTK);
	free(groupBASIC_CONNECT);
	free(groupSMS);
	free(groupUSSD);
	free(groupPHONEBOOK);
	free(groupSTK);
	return ret;
}

mbim_message_t *mbim_format_suscriber_ready_status() {
	mbim_message_t *msg = calloc(1, sizeof(mbim_message_t));

	uint32_t code = mbim_get_cmd_code(MBIM_CID_SUBSCRIBER_READY_STATUS);
	UUID_t uuid = mbim_get_uuid(MBIM_CID_SUBSCRIBER_READY_STATUS);
	uint32_t command_type = 0; // query

	msg->type = MBIM_COMMAND_MSG;
	msg->hex_buf = strdup_printf("%s " FUINT32LE FUINT32LE FUINT32LE, uuid, VUINT32LE(code), VUINT32LE(command_type), VUINT32LE(0));

	return msg;
}

mbim_message_t *mbim_format_set_connect(
		uint32_t sessionId,
		uint32_t activation,
		const char *apn,
		uint32_t auth_method,
		const char *auth_user,
		const char *auth_pwd,
		uint32_t compression,
		uint32_t ip_type,
		UUID_t context_type) {
	mbim_message_t *msg = NULL;
	char *mbim_apn = NULL;
	char *mbim_auth_user = NULL;
	char *mbim_auth_pwd = NULL;
	size_t apn_len, auth_user_len, auth_pwd_len, fix_len;

	uint32_t code = mbim_get_cmd_code(MBIM_CID_CONNECT);
	UUID_t uuid = mbim_get_uuid(MBIM_CID_CONNECT);
	uint32_t command_type = 1; // set

	mbim_apn = mbim_get_string(apn);
	if(!mbim_apn || strlen(mbim_apn)>400) goto error;
	mbim_auth_user = mbim_get_string(auth_user);
	if(!mbim_auth_user || strlen(mbim_auth_user)>1020) goto error;
	mbim_auth_pwd = mbim_get_string(auth_pwd);
	if(!mbim_auth_pwd || strlen(mbim_auth_pwd)>1020) goto error;

	msg = calloc(1, sizeof(mbim_message_t));
	msg->type = MBIM_COMMAND_MSG;

	const char *apn_pad = "";
	const char *auth_user_pad = "";
	const char *auth_pwd_pad = "";
	apn_len = strlen(mbim_apn)/2;
	if(apn_len%4) apn_pad = "0000";
	auth_user_len = strlen(mbim_auth_user)/2;
	if(auth_user_len%4) auth_user_pad = "0000";
	auth_pwd_len = strlen(mbim_auth_pwd)/2;
	if(auth_pwd_len%4) auth_pwd_pad = "0000";

	fix_len = 11*4+UUID_LEN;
	char *buffer = strdup_printf(FUINT32LE FUINT32LE FUINT32LE FUINT32LE // sessionId, activation, apn_poslen
		FUINT32LE FUINT32LE FUINT32LE FUINT32LE	// authuser_poslen, authpwd_poslen
		FUINT32LE FUINT32LE FUINT32LE "%s "	// compression, auth_method, ip_type, context_type
		"%s%s %s%s %s%s",		// databuffer: apn+pad. auth_user+pad, auth_pwd+pad
		VUINT32LE(sessionId), VUINT32LE(activation),
		VUINT32LE(apn_len?fix_len:0), VUINT32LE(apn_len),
		VUINT32LE(auth_user_len?fix_len+apn_len+strlen(apn_pad)/2:0), VUINT32LE(auth_user_len),
		VUINT32LE(auth_pwd_len?fix_len+apn_len+strlen(apn_pad)/2+auth_user_len+strlen(auth_user_pad)/2:0), VUINT32LE(auth_pwd_len),
		VUINT32LE(compression), VUINT32LE(auth_method), VUINT32LE(ip_type), context_type,
		mbim_apn, apn_pad, mbim_auth_user, auth_user_pad, mbim_auth_pwd, auth_pwd_pad
	);

	msg->hex_buf = strdup_printf("%s " FUINT32LE FUINT32LE FUINT32LE "%s", uuid, VUINT32LE(code), VUINT32LE(command_type), VUINT32LE(hex_to_bin_len(buffer)), buffer);
	free(buffer);
	return msg;

error:
	if(mbim_apn) free(mbim_apn);
	if(mbim_auth_user) free(mbim_auth_user);
	if(mbim_auth_pwd) free(mbim_auth_pwd);
	if(msg) free(msg);
	return NULL;
}

mbim_message_t *mbim_format_query_ip_configuration(uint32_t sessionId) {
	mbim_message_t *msg = calloc(1, sizeof(mbim_message_t));

	uint32_t code = mbim_get_cmd_code(MBIM_CID_IP_CONFIGURATION);
	UUID_t uuid = mbim_get_uuid(MBIM_CID_IP_CONFIGURATION);
	uint32_t command_type = 0; // query

	msg->type = MBIM_COMMAND_MSG;

	char *infobuf = calloc(120+1, sizeof(char));
	sprintf(infobuf,FUINT32LE, VUINT32LE(sessionId));
	memset(infobuf+8, '0', 112);
	DBG("%s", infobuf)

	msg->hex_buf = strdup_printf("%s " FUINT32LE FUINT32LE FUINT32LE "%s", uuid, VUINT32LE(code), VUINT32LE(command_type), VUINT32LE(60), infobuf);

	free(infobuf);

	return msg;
}


void mbim_free_frame(mbim_frame_t *frame) {
	while(frame) {
		mbim_frame_t *next = frame->next;
		if(frame->data)
			free(frame->data);
		free(frame);
		frame = next;
	}
}

uint32_t mbim_get_frame_msg_type(const unsigned char* frame) {
	return bin_to_uint32(frame, USE_LITTLE_ENDIAN);
}

uint32_t mbim_get_frame_length(const unsigned char* frame) {
	return bin_to_uint32(frame+4, USE_LITTLE_ENDIAN);
}

uint32_t mbim_get_frame_sequence_id(const unsigned char* frame) {
	return bin_to_uint32(frame+8, USE_LITTLE_ENDIAN);
}

uint32_t mbim_get_frame_fragments(const unsigned char* frame) {
	uint32_t type = mbim_get_frame_msg_type(frame);
	if(type!=MBIM_COMMAND_DONE && type!=MBIM_INDICATE_STATUS_MSG)
		return 1;
	return bin_to_uint32(frame+12, USE_LITTLE_ENDIAN);
}

uint32_t mbim_get_frame_current_fragment(const unsigned char* frame) {
	uint32_t type = mbim_get_frame_msg_type(frame);
	if(type!=MBIM_COMMAND_DONE && type!=MBIM_INDICATE_STATUS_MSG)
		return 0;
	return bin_to_uint32(frame+16, USE_LITTLE_ENDIAN);
}

void print_mbim_frame(unsigned char *buf) {
	uint32_t frame_length = mbim_get_frame_length(buf);
	print_hexa(buf, frame_length);
}

// TODO: split the command type==MBIM_COMMAND_MSG in frames if size>MaxControlTransfer
mbim_frame_t *mbim_message_to_frames(mbim_message_t *msg, uint32_t sequenceId, uint32_t MaxControlTransfer) {
	mbim_frame_t *frame = calloc(1, sizeof(mbim_frame_t));
	char *buf;
	switch(msg->type) {
	case MBIM_OPEN:
		buf = strdup_printf(FUINT32LE FUINT32LE FUINT32LE		// type, message_length, sequenceId
			FUINT32LE, 						// MaxControlTransfer
			VUINT32LE(msg->type), VUINT32LE(4*sizeof(uint32_t)), VUINT32LE(sequenceId),
			VUINT32LE(MaxControlTransfer));
		break;
	case MBIM_CLOSE:
		buf = strdup_printf(FUINT32LE FUINT32LE FUINT32LE,		// type, message_length, sequenceId
			VUINT32LE(msg->type), VUINT32LE(3*sizeof(uint32_t)), VUINT32LE(sequenceId));
		break;
	case MBIM_COMMAND_MSG:
		buf = strdup_printf(FUINT32LE FUINT32LE FUINT32LE		// type, message_length, sequenceId
			FUINT32LE FUINT32LE					// totalFragments=1, currentFragment=0,
			"%s", 							// buffer, including DeviceServiceUUID, CID, CommandType, InformationBufferLength and InformationBuffer
			VUINT32LE(msg->type), VUINT32LE(5*sizeof(uint32_t)+hex_to_bin_len(msg->hex_buf)), VUINT32LE(sequenceId),
			VUINT32LE(1), VUINT32LE(0),
			msg->hex_buf);
		break;
	default: // invalid or not supported type
		free(frame);
		return NULL;
		break;
	}
	if(hex_to_bin_len(buf)==-INCOMPLETE_HEX_VALUE)
		exit(2);
	frame->data = calloc(hex_to_bin_len(buf), sizeof(unsigned char));
	hex_to_bin(buf, frame->data);
	free(buf);
	return frame;
}

typedef struct {
	enum mbim_command_code cc;
	UUID_t uuid;
	uint32_t CID;
} mbim_command_t;

static mbim_command_t mbim_commands[] = {
	{ MBIM_CID_DEVICE_CAPS,			UUID_BASIC_CONNECT,	1 },
	{ MBIM_CID_SUBSCRIBER_READY_STATUS,	UUID_BASIC_CONNECT,	2 },
	{ MBIM_CID_RADIO_STATE,			UUID_BASIC_CONNECT,	3 },
	{ MBIM_CID_PIN,				UUID_BASIC_CONNECT,	4 },
	{ MBIM_CID_PIN_LIST,			UUID_BASIC_CONNECT,	5 },
	{ MBIM_CID_HOME_PROVIDER,		UUID_BASIC_CONNECT,	6 },
	{ MBIM_CID_PREFERRED_PROVIDERS,		UUID_BASIC_CONNECT,	7 },
	{ MBIM_CID_VISIBLE_PROVIDERS,		UUID_BASIC_CONNECT,	8 },
	{ MBIM_CID_REGISTER_STATE,		UUID_BASIC_CONNECT,	9 },
	{ MBIM_CID_PACKET_SERVICE,		UUID_BASIC_CONNECT,	10 },
	{ MBIM_CID_SIGNAL_STATE,		UUID_BASIC_CONNECT,	11 },
	{ MBIM_CID_CONNECT,			UUID_BASIC_CONNECT,	12 },
	{ MBIM_CID_PROVISIONED_CONTEXTS,	UUID_BASIC_CONNECT,	13 },
	{ MBIM_CID_SERVICE_ACTIVATION,		UUID_BASIC_CONNECT,	14 },
	{ MBIM_CID_IP_CONFIGURATION,		UUID_BASIC_CONNECT,	15 },
	{ MBIM_CID_DEVICE_SERVICES,		UUID_BASIC_CONNECT,	16 },
	{ MBIM_CID_DEVICE_SERVICE_SUBSCRIBE_LIST,UUID_BASIC_CONNECT,	19 },
	{ MBIM_CID_PACKET_STATISTICS,		UUID_BASIC_CONNECT,	20 },
	{ MBIM_CID_NETWORK_IDLE_HINT,		UUID_BASIC_CONNECT,	21 },
	{ MBIM_CID_EMERGENCY_MODE,		UUID_BASIC_CONNECT,	22 },
	{ MBIM_CID_IP_PACKET_FILTERS,		UUID_BASIC_CONNECT,	23 },
	{ MBIM_CID_MULTICARRIER_PROVIDERS,	UUID_BASIC_CONNECT,	24 },
	{ MBIM_CID_SMS_CONFIGURATION,		UUID_SMS,		1 },
	{ MBIM_CID_SMS_READ,			UUID_SMS,		2 },
	{ MBIM_CID_SMS_SEND,			UUID_SMS,		3 },
	{ MBIM_CID_SMS_DELETE,			UUID_SMS,		4 },
	{ MBIM_CID_SMS_MESSAGE_STORE_STATUS,	UUID_SMS,		5 },
	{ MBIM_CID_USSD,			UUID_USSD,		1 },
	{ MBIM_CID_PHONEBOOK_CONFIGURATION,	UUID_PHONEBOOK,		1 },
	{ MBIM_CID_PHONEBOOK_READ,		UUID_PHONEBOOK,		2 },
	{ MBIM_CID_PHONEBOOK_DELETE,		UUID_PHONEBOOK,		3 },
	{ MBIM_CID_PHONEBOOK_WRITE,		UUID_PHONEBOOK,		4 },
	{ MBIM_CID_STK_PAC,			UUID_STK,		1 },
	{ MBIM_CID_STK_TERMINAL_RESPONSE,	UUID_STK,		2 },
	{ MBIM_CID_STK_ENVELOPE,		UUID_STK,		3 },
	{ MBIM_CID_AKA_AUTH,			UUID_AUTH,		1 },
	{ MBIM_CID_AKAP_AUTH,			UUID_AUTH,		2 },
	{ MBIM_CID_SIM_AUTH,			UUID_AUTH,		3 },
	{ MBIM_CID_DSS_CONNECT,			UUID_DSS,		1 },
 };

uint32_t mbim_get_cmd_code(enum mbim_command_code cc) {
	return mbim_commands[cc].CID;
};

UUID_t mbim_get_uuid(enum mbim_command_code cc) {
	return mbim_commands[cc].uuid;
};

enum mbim_command_code mbim_get_msg_cmd_code(mbim_function_message_t* msg) {
	enum mbim_command_code cc = MBIM_CID_DEVICE_CAPS;
	unsigned char uuid_bin[UUID_LEN];
	while(cc<MBIM_INVALID) {
		hex_to_bin(mbim_commands[cc].uuid, uuid_bin);
		uint32_t CID = bin_to_uint32(msg->bin_buf+UUID_LEN, USE_LITTLE_ENDIAN);
		if(memcmp(uuid_bin, msg->bin_buf, UUID_LEN)==0 && CID==mbim_commands[cc].CID)
			return cc;
		++cc;
	}
	return cc;
}

const char *act_strings[5] = {
	"MBIMActivationStateUnknown",
	"MBIMActivationStateActivated",
	"MBIMActivationStateActivating",
	"MBIMActivationStateDeactivated",
	"MBIMActivationStateDeactivating"
};

const char*get_activation_state_string(uint32_t act_state) {
	if(act_state>=5)
		act_state=0;
	return act_strings[act_state];
}
