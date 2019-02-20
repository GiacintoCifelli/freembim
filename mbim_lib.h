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

#ifndef __MBIM_LIB_H__
#define __MBIM_LIB_H__

#include <stdint.h>

typedef const unsigned char* UUID_t;

enum mbim_message_type {
	// host messages
	MBIM_OPEN		= 0x00000001,
	MBIM_CLOSE		= 0x00000002,
	MBIM_COMMAND_MSG 	= 0x00000003,
	MBIM_HOST_ERROR		= 0x00000004,
	// function messages: answers
	MBIM_OPEN_DONE		= 0x80000001,
	MBIM_CLOSE_DONE		= 0x80000002,
	MBIM_COMMAND_DONE 	= 0x80000003,
	MBIM_FUNCTION_ERROR_MSG	= 0x80000004,
	// function message: event
	MBIM_INDICATE_STATUS_MSG= 0x80000007,
};

typedef struct {
	uint32_t type;
	char *hex_buf;
} mbim_message_t;

typedef struct {
	uint32_t type;
	uint32_t sequence_id;
	uint32_t size;
	unsigned char*bin_buf;
} mbim_function_message_t;

char *mbim_get_string(const unsigned char*utf8);

void mbim_free_message(mbim_message_t *msg);
void mbim_free_function_message(mbim_function_message_t *msg);
mbim_message_t *mbim_format_open();
mbim_message_t *mbim_format_query_device_capabilities();
mbim_message_t *mbim_format_set_subscriptions(int ElementCount, ...);
char *mbim_get_subscription_group(UUID_t uuid, uint32_t CidCount, ...);
mbim_message_t *mbim_format_set_all_subscriptions();
mbim_message_t *mbim_format_close();
mbim_message_t *mbim_format_suscriber_ready_status();
mbim_message_t *mbim_format_set_connect(
		uint32_t sessionId,
		uint32_t activation,
		const char *apn,
		uint32_t auth_method,
		const char *auth_user,
		const char *auth_pwd,
		uint32_t compression,
		uint32_t ip_type,
		UUID_t context_type);
mbim_message_t *mbim_format_query_ip_configuration(uint32_t sessionId);


typedef struct {
	uint32_t MessageType;
	uint32_t MessageLength;
	uint32_t TransactionId;
} mbim_message_header_t;

typedef struct {
	uint32_t TotalFragments;
	uint32_t CurrentFragment;
} mbim_fragment_header_t;

// commands

typedef struct {
	mbim_message_header_t header;
	uint32_t MaxControlTransfer;
} mbim_open_msg_t;

typedef struct {
	mbim_message_header_t header;
} mbim_close_msg_t;

typedef struct {
	mbim_message_header_t header;
	mbim_fragment_header_t fragment;
	UUID_t DeviceServiceId;
	uint32_t CID;
	uint32_t CommandType;
	uint32_t InformationBufferLength;
	void* InformationBuffer;
} mbim_command_msg_t;

/******************************************************************************/
// errors

typedef struct {
	mbim_message_header_t header;
	uint32_t ErrorStatusCode;
} mbim_error_msg_t;

/******************************************************************************/
// responses

typedef struct {
	mbim_message_header_t header;
	uint32_t Status;
} mbim_open_done_msg_t;

typedef struct {
	mbim_message_header_t header;
	uint32_t Status;
} mbim_close_done_msg_t;

typedef struct {
	mbim_message_header_t header;
	mbim_fragment_header_t fragment;
	UUID_t DeviceServiceId;
	uint32_t CID;
	uint32_t Status;
	uint32_t InformationBufferLength;
	void* InformationBuffer;
} mbim_command_done_msg_t;

/******************************************************************************/
// events

typedef struct {
	mbim_message_header_t header;
	mbim_fragment_header_t fragment;
	UUID_t DeviceServiceId;
	uint32_t CID;
	uint32_t InformationBufferLength;
	void* InformationBuffer;
} mbim_indicate_status_msg_t;

/******************************************************************************/

enum mbim_status_codes {
	MBIM_STATUS_SUCCESS				= 0,
	MBIM_STATUS_BUSY				= 1,
	MBIM_STATUS_FAILURE				= 2,
	MBIM_STATUS_SIM_NOT_INSERTED 			= 3,
	MBIM_STATUS_BAD_SIM				= 4,
	MBIM_STATUS_PIN_REQUIRED			= 5,
	MBIM_STATUS_PIN_DISABLED			= 6,
	MBIM_STATUS_NOT_REGISTERED			= 7,
	MBIM_STATUS_PROVIDERS_NOT_FOUND			= 8,
	MBIM_STATUS_NO_DEVICE_SUPPORT			= 9,
	MBIM_STATUS_PROVIDER_NOT_VISIBLE		= 10,
	MBIM_STATUS_DATA_CLASS_NOT_AVAILABLE		= 11,
	MBIM_STATUS_PACKET_SERVICE_DETACHED		= 12,
	MBIM_STATUS_MAX_ACTIVATED_CONTEXTS		= 13,
	MBIM_STATUS_NOT_INITIALIZED			= 14,
	MBIM_STATUS_VOICE_CALL_IN_PROGRESS		= 15,
	MBIM_STATUS_CONTEXT_NOT_ACTIVATED		= 16,
	MBIM_STATUS_SERVICE_NOT_ACTIVATED		= 17,
	MBIM_STATUS_INVALID_ACCESS_STRING		= 18,
	MBIM_STATUS_INVALID_USER_NAME_PWD		= 19,
	MBIM_STATUS_RADIO_POWER_OFF			= 20,
	MBIM_STATUS_INVALID_PARAMETERS			= 21,
	MBIM_STATUS_READ_FAILURE			= 22,
	MBIM_STATUS_WRITE_FAILURE			= 23,
	/* Reserved					= 24, */
	MBIM_STATUS_NO_PHONEBOOK			= 25,
	MBIM_STATUS_PARAMETER_TOO_LONG			= 26,
	MBIM_STATUS_STK_BUSY				= 27,
	MBIM_STATUS_OPERATION_NOT_ALLOWED		= 28,
	MBIM_STATUS_MEMORY_FAILURE			= 29,
	MBIM_STATUS_INVALID_MEMORY_INDEX		= 30,
	MBIM_STATUS_MEMORY_FULL				= 31,
	MBIM_STATUS_FILTER_NOT_SUPPORTED		= 32,
	MBIM_STATUS_DSS_INSTANCE_LIMIT			= 33,
	MBIM_STATUS_INVALID_DEVICE_SERVICE_OPERATION	= 34,
	MBIM_STATUS_AUTH_INCORRECT_AUTN			= 35,
	MBIM_STATUS_AUTH_SYNC_FAILURE			= 36,
	MBIM_STATUS_AUTH_AMF_NOT_SET			= 37,
	MBIM_STATUS_CONTEXT_NOT_SUPPORTED		= 38,
	MBIM_STATUS_SMS_UNKNOWN_SMSC_ADDRESS		= 100,
	MBIM_STATUS_SMS_NETWORK_TIMEOUT			= 101,
	MBIM_STATUS_SMS_LANG_NOT_SUPPORTED		= 102,
	MBIM_STATUS_SMS_ENCODING_NOT_SUPPORTED		= 103,
	MBIM_STATUS_SMS_FORMAT_NOT_SUPPORTED		= 104
	/* Device service specific status commands: 0x80000000-0xFFFFFFFF */
};

#define UUID_LEN		(16)

#define UUID_BASIC_CONNECT_STR	"Basic IP Connectivity"
#define UUID_BASIC_CONNECT	"a289cc33-bcbb-8b4f-b6b0-133ec2aae6df"
#define UUID_SMS_STR		"SMS"
#define UUID_SMS		"533fbeeb-14fe-4467-9f90-33a223e56c3f"
#define UUID_USSD_STR		"USSD (Unstructured Supplementary Service Data)"
#define UUID_USSD		"e550a0c8-5e82-479e-82f7-10abf4c3351f"
#define UUID_PHONEBOOK_STR	"Phonebook"
#define UUID_PHONEBOOK		"4bf38476-1e6a-41db-b1d8-bed289c25bdb"
#define UUID_STK_STR		"STK (SIM Toolkit)"
#define UUID_STK		"d8f20131-fcb5-4e17-8602-d6ed3816164c"
#define UUID_AUTH_STR		"Authentication"
#define UUID_AUTH		"1d2b5ff7-0aa1-48b2-aa52-50f15767174e"
#define UUID_DSS_STR		"Device Service Stream"
#define UUID_DSS		"c08a26dd-7718-4382-8482-6e0d583c4d0e"

enum mbim_command_code {
	MBIM_CID_DEVICE_CAPS				= 0,
	MBIM_CID_SUBSCRIBER_READY_STATUS,
	MBIM_CID_RADIO_STATE,
	MBIM_CID_PIN,
	MBIM_CID_PIN_LIST,
	MBIM_CID_HOME_PROVIDER,
	MBIM_CID_PREFERRED_PROVIDERS,
	MBIM_CID_VISIBLE_PROVIDERS,
	MBIM_CID_REGISTER_STATE,
	MBIM_CID_PACKET_SERVICE,
	MBIM_CID_SIGNAL_STATE,
	MBIM_CID_CONNECT,
	MBIM_CID_PROVISIONED_CONTEXTS,
	MBIM_CID_SERVICE_ACTIVATION,
	MBIM_CID_IP_CONFIGURATION,
	MBIM_CID_DEVICE_SERVICES,
	MBIM_CID_DEVICE_SERVICE_SUBSCRIBE_LIST,
	MBIM_CID_PACKET_STATISTICS,
	MBIM_CID_NETWORK_IDLE_HINT,
	MBIM_CID_EMERGENCY_MODE,
	MBIM_CID_IP_PACKET_FILTERS,
	MBIM_CID_MULTICARRIER_PROVIDERS,
	MBIM_CID_SMS_CONFIGURATION,
	MBIM_CID_SMS_READ,
	MBIM_CID_SMS_SEND,
	MBIM_CID_SMS_DELETE,
	MBIM_CID_SMS_MESSAGE_STORE_STATUS,
	MBIM_CID_USSD,
	MBIM_CID_PHONEBOOK_CONFIGURATION,
	MBIM_CID_PHONEBOOK_READ,
	MBIM_CID_PHONEBOOK_DELETE,
	MBIM_CID_PHONEBOOK_WRITE,
	MBIM_CID_STK_PAC,
	MBIM_CID_STK_TERMINAL_RESPONSE,
	MBIM_CID_STK_ENVELOPE,
	MBIM_CID_AKA_AUTH,
	MBIM_CID_AKAP_AUTH,
	MBIM_CID_SIM_AUTH,
	MBIM_CID_DSS_CONNECT,
	MBIM_INVALID
};

uint32_t mbim_get_cmd_code(enum mbim_command_code cc);
UUID_t mbim_get_uuid(enum mbim_command_code cc);

/******************************************************************************/

// MBIM_CID_DEVICE_CAPS response
typedef struct {
	uint32_t DeviceType;
	uint32_t CellularClass;
	uint32_t VoiceClass;
	uint32_t SimClass;
	uint32_t DataClass;
	uint32_t SmsCaps;
	uint32_t ControlCaps;
	uint32_t MaxSessions;
	uint32_t CustomDataClassOffset;
	uint32_t CustomDataClassSize;
	uint32_t DeviceIdOffset;
	uint32_t DeviceIdSize;
	uint32_t FirmwareInfoOffset;
	uint32_t FirmwareInfoSize;
	uint32_t HardwareInfoOffset;
	uint32_t HardwareInfoSize;
	void *DataBuffer;
} mbim_device_caps_info_t;

// MBIM_CID_DEVICE_SERVICE_SUBSCRIBE_LIST command and response

typedef struct {
	uint32_t offset;
	uint32_t length;
} mbim_ol_pair;

typedef struct {
	UUID_t DeviceServiceId;
	uint32_t CidCount;
	uint32_t DataBuffer[]; // The list of CidCount CID values
} mbim_event_entry;

typedef struct {
	uint32_t ElementCount;
	mbim_ol_pair DeviceServiceSubscribeRefList[7];
	mbim_event_entry DataBuffer[7];
} mbim_device_subscribe_list_t;

// MBIM_CID_CONNECT -> MBIM_SET_CONNECT: command and response structures

#define MBIMContextTypeNone		"B43F758C-A560-4B46-B35E-C5869641FB54"
#define MBIMContextTypeInternet		"7E5E2A7E-4E6F-7272-736B-656E7E5E2A7E"
#define MBIMContextTypeVpn		"9B9F7BBE-8952-44B7-83AC-CA41318DF7A0"
#define MBIMContextTypeVoice		"88918294-0EF4-4396-8CCA-A8588FBC02B2"
#define MBIMContextTypeVideoShare	"05A2A716-7C34-4B4D-9A91-C5EF0C7AAACC"
#define MBIMContextTypePurchase		"B3272496-AC6C-422B-A8C0-ACF687A27217"
#define MBIMContextTypeIMS		"21610D01-3074-4BCE-9425-B53A07D697D6"
#define MBIMContextTypeMMS		"46726664-7269-6BC6-9624-D1D35389ACA9"
#define MBIMContextTypeLocal		"A57A9AFC-B09F-45D7-BB40-033C39F60DB9"

typedef struct {
	uint32_t SessionId;
	uint32_t ActivationCommand;
	uint32_t AccessStringOffset;
	uint32_t AccessStringSize;
	uint32_t UserNameOffset;
	uint32_t UserNameSize;
	uint32_t PasswordOffset;
	uint32_t PasswordSize;
	uint32_t Compression;
	uint32_t AuthProtocol;
	uint32_t IPType;
	UUID_t ContextType;
	void *DataBuffer; // AccessString, UserName, Password
} mbim_set_connect;

// MBIM_CID_CONNECT -> MBIM_CONNECT_INFO

typedef struct {
	uint32_t SessionId;
	uint32_t ActivationState;
	uint32_t VoiceCallState;
	uint32_t IPType;
	UUID_t ContextType;
	uint32_t NwError;
} mbim_connect_info;

// MBIM_CID_IP_CONFIGURATION -> MBIM_IP_CONFIGURATION_INFO: event structure

typedef uint8_t mbim_ipv4_address[4];
typedef struct {
	uint32_t OnLinkPrefixLength;
	mbim_ipv4_address IPv4Address;
} mbim_ipv4_element;

typedef uint8_t mbim_ipv6_address[16];
typedef struct {
	uint32_t OnLinkPrefixLength;
	mbim_ipv6_address IPv6Address;
} mbim_ipv6_element;

typedef struct {
	uint32_t SessionId; // only this set for the query
	uint32_t IPv4ConfigurationAvailable; // Bit 0: IPv4 Address info available, Bit 1: IPv4 gateway info available, Bit 2: IPv4 DNS server info available, Bit 3: IPv4 MTU info available
	uint32_t IPv6ConfigurationAvailable; // same as above
	uint32_t IPv4AddressCount;
	uint32_t IPv4AddressOffset; // MBIM_IPV4_ELEMENT[]
	uint32_t IPv6AddressCount;
	uint32_t IPv6AddressOffset; // MBIM_IPV6_ELEMENT[]
	uint32_t IPv4GatewayOffset; // MBIM_IPV4_ADDRESS;
	uint32_t IPv6GatewayOffset; // MBIM_IPV6_ADDRESS
	uint32_t IPv4DnsServerCount;
	uint32_t IPv4DnsServerOffset; // MBIM_IPV4_ADDRESS
	uint32_t IPv6DnsServerCount;
	uint32_t IPv6DnsServerOffset; // MBIM_IPV6_ADDRESS
	uint32_t IPv4Mtu;
	uint32_t IPv6Mtu;
	void *DataBuffer;
} mbim_ip_configuration_info_t;

const char*get_activation_state_string(uint32_t act_state);

/******************************************************************************/

// for MBIM thread exclusive use:
// general sending frame structures
// completed with sequenceID and split in frames (using the device MaxControlTransfer size) by thread_mbim

typedef struct mbim_frame_t mbim_frame_t;
struct mbim_frame_t{
	unsigned char* data;
	mbim_frame_t *next;
};

void mbim_free_frame(mbim_frame_t *frame);
uint32_t mbim_get_frame_msg_type(const unsigned char* frame);
uint32_t mbim_get_frame_length(const unsigned char* frame);
uint32_t mbim_get_frame_sequence_id(const unsigned char* frame);

uint32_t mbim_get_frame_fragments(const unsigned char* frame);
uint32_t mbim_get_frame_current_fragment(const unsigned char* frame);

void print_mbim_frame(unsigned char *buf);

enum mbim_command_code mbim_get_msg_cmd_code(mbim_function_message_t* msg);

mbim_frame_t *mbim_message_to_frames(mbim_message_t *msg, uint32_t sequenceId, uint32_t MaxControlTransfer);
mbim_message_t *mbim_frames_to_message(mbim_frame_t *frame);

#endif /* __MBIM_LIB_H__ */
