#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <map>
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>

#define SOCK_ERROR()	WSAGetLastError()
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/times.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define SOCK_ERROR()	errno
#endif


#define CHECK_BREAK(x)	if (!x) { break; }
#define CHECK_STATUS(x, status, value) if (!x) {status = value; break;}

#define RTMP_DEFAULT_PORT				1935

#define RTMP_HANDSHAKE_SIG_SIZE			1536
#define RTMP_VERSION					0x03


typedef enum _rtmp_proto
{
	RTMP_PROTOCOL_RTMP = 0x00,
	RTMP_PROTOCOL_RTMPT	= 0x01, // Not support yet
	RTMP_PROTOCOL_RTMPE = 0x02  // Not support yet
} rtmp_proto_t;

typedef enum _rtmp_msg_type
{
	RTMP_MSG_TYPE_RESERVED00 = 0x00,
	RTMP_MSG_TYPE_CHUNK_SIZE = 0x01,
	RTMP_MSG_TYPE_RESERVED02 = 0x02,
	RTMP_MSG_TYPE_BYTES_READ_REPORT = 0x03,
	RTMP_MSG_TYPE_CONTROL = 0x04,
	RTMP_MSG_TYPE_SERVER_BW = 0x05,
	RTMP_MSG_TYPE_CLIENT_BW = 0x06,
	RTMP_MSG_TYPE_RESERVED07 = 0x07,
	RTMP_MSG_TYPE_AUDIO = 0x08,
	RTMP_MSG_TYPE_VIDEO = 0x09,
	RTMP_MSG_TYPE_RESERVED0A = 0x0A,
	RTMP_MSG_TYPE_RESERVED0B = 0x0B,
	RTMP_MSG_TYPE_RESERVED0C = 0x0C,
	RTMP_MSG_TYPE_RESERVED0D = 0x0D,
	RTMP_MSG_TYPE_RESERVED0E = 0x0E,
	RTMP_MSG_TYPE_FLEX_STREAM_SEND = 0x0F,
	RTMP_MSG_TYPE_FLEX_SHARED_OBJECT = 0x10,
	RTMP_MSG_TYPE_FLEX_MESSAGE = 0x11,
	RTMP_MSG_TYPE_INFO = 0x12,
	RTMP_MSG_TYPE_SHARED_OBJECT = 0x13,
	RTMP_MSG_TYPE_INVOKE = 0x14,
	RTMP_MSG_TYPE_RESERVED15 = 0x15,
	RTMP_MSG_TYPE_FLASH_VIDEO = 0x16
} rtmp_msg_type_t;

typedef enum _rtmp_chunk_type
{
	RTMP_CHUNK_TYPE_LARGE = 0x00,
	RTMP_CHUNK_TYPE_MEDIUM,
	RTMP_CHUNK_TYPE_SMALL,
	RTMP_CHUNK_TYPE_MINIMUM
} rtmp_chunk_type_t;

typedef enum _rtmp_chunk_stream
{
	RTMP_CHUNK_STREAM_RESERVED00 = 0x00,
	RTMP_CHUNK_STREAM_RESERVED01 = 0x01,
	RTMP_CHUNK_STREAM_PROTOCOL_CONTROL = 0x02,
	RTMP_CHUNK_STREAM_OVER_CONNECTION = 0x03,
	RTMP_CHUNK_STREAM_OVER_CONNECTION2 = 0x04,
	RTMP_CHUNK_STREAM_OVER_STREAM = 0x05,
	RTMP_CHUNK_STREAM_VIDEO = 0x06,
	RTMP_CHUNK_STREAM_AUDIO = 0x07,
	RTMP_CHUNK_STREAM_OVER_STREAM2 = 0x08
} rtmp_chunk_stream_t;


typedef struct _rtmp_context
{
	rtmp_proto_t protocol;
	std::string host;
	uint32_t port;
	std::string app;
	std::string stream;

	int socket;
	int stream_id;
	uint64_t in_chunk_size;
	uint64_t out_chunk_size;
	uint64_t in_bytes;
	uint64_t out_bytes;
} rtmp_context_t;

// Message type/Message stream id ----> chunks
typedef struct _rtmp_packet
{
	rtmp_msg_type_t msg_type;
	int msg_stream_id; // = stream_id or ignore(0)

	rtmp_chunk_type_t chk_type; // Only support RTMP_CHUNK_TYPE_LARGE for public
	rtmp_chunk_stream_t chk_stream_id;
	uint32_t timestamp;

	uint32_t size;
	uint32_t valid;
	uint8_t *data_ptr;
} rtmp_packet_t;

typedef struct _rtmp_invoke
{

} rtmp_invoke_t;

