#pragma once
#pragma warning(disable: 4996)
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <list>
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
#define RTMP_DEFAULT_CHUNK_SIZE			128
#define RTMP_DEFAULT_BW					2500000

#define RTMP_HANDSHAKE_SIG_SIZE			1536
#define RTMP_VERSION					0x03

#define RTMP_MAX_HEADER_SIZE			18 // Basic Header(3) + Msg Header(11) + Extend Timestamp(4)

#define RTMP_MAX_CHUNK_SIZE				(2 * 1024 * 1024)				


typedef enum _rtmp_proto
{
	RTMP_PROTOCOL_RTMP = 0x00,
	RTMP_PROTOCOL_RTMPT	= 0x01, // Not support yet
	RTMP_PROTOCOL_RTMPE = 0x02  // Not support yet
} rtmp_proto_t;

typedef enum _rtmp_mode
{
	RTMP_MODE_PUSHER = 0x00,
	RTMP_MODE_PULLER
} rtmp_mode_t;

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


// Message type/Message stream id ----> split to chunks
typedef struct _rtmp_packet
{
	rtmp_chunk_type_t chk_type;
	uint32_t chk_stream_id;

	rtmp_msg_type_t msg_type;
	uint32_t msg_stream_id; // = stream_id or ignore(0)
	uint32_t timestamp;

	uint32_t size;
	uint32_t valid;
	uint8_t *data_ptr;
} rtmp_packet_t;


typedef struct _rtmp_link
{
	rtmp_proto_t protocol;
	std::string host;
	uint32_t port;
	std::string app;
	std::string stream;
} rtmp_link_t;

typedef struct _rtmp_param
{
	std::string flashVer;
	std::string swfUrl;
	std::string tcUrl;
	std::string pageUrl; // Only for puller
	uint64_t encoding; // AMF0 or AMF3
	std::string auth;
	//amf_object_t extras;
} rtmp_param_t;

typedef struct _rtmp_invoke
{
	uint64_t id;
	std::string invoke;
} rtmp_invoke_t;

typedef struct _rtmp_context
{
	rtmp_link_t link;
	rtmp_param_t params;
	rtmp_mode_t mode;

	int socket;
	bool playing;
	uint32_t stream_id;
	uint32_t in_chunk_size;
	uint32_t out_chunk_size;
	uint32_t server_bw;
	uint32_t client_bw;
	uint32_t client_bw2;
	uint64_t in_bytes_count;
	uint64_t out_bytes_count;

	uint64_t invoke_ids;
	std::vector<rtmp_invoke_t> invokes;
	std::map<uint32_t, rtmp_packet_t> in_channels;
	std::map<uint32_t, rtmp_packet_t> out_channels;
} rtmp_context_t;


#define H264_PARAM_LEN		128
typedef struct _h264_param
{
	uint32_t size_sps;
	uint8_t	data_sps[H264_PARAM_LEN];
	uint32_t size_pps;
	uint8_t	data_pps[H264_PARAM_LEN];
} h264_param_t;

typedef struct _rtmp_metadata
{
	// Video
	uint32_t width;
	uint32_t height;
	uint32_t fps;
	uint32_t bitrate_kpbs;
	h264_param_t param;

	// Audio
	bool has_audio;
	uint32_t channels;
	uint32_t samplerate;
	uint32_t samples_per_frame;
	uint32_t datarate;
} rtmp_metadata_t;


static inline void rtmp_dump_packet(const rtmp_packet_t *pkt_ptr)
{

}

static inline void rtmp_init_packet(rtmp_packet_t *pkt_ptr)
{
	pkt_ptr->chk_type = RTMP_CHUNK_TYPE_LARGE;
	pkt_ptr->chk_stream_id = 0;
	pkt_ptr->msg_type = RTMP_MSG_TYPE_RESERVED00;
	pkt_ptr->msg_stream_id = 0;
	pkt_ptr->timestamp = 0;
	pkt_ptr->size = 0;
	pkt_ptr->valid = 0;
	pkt_ptr->data_ptr = NULL;
}

static inline void rtmp_copy_packet(rtmp_packet_t *dst_pkt_ptr, const rtmp_packet_t *src_pkt_ptr)
{
	dst_pkt_ptr->chk_type = src_pkt_ptr->chk_type;
	dst_pkt_ptr->chk_stream_id = src_pkt_ptr->chk_stream_id;
	dst_pkt_ptr->msg_type = src_pkt_ptr->msg_type;
	dst_pkt_ptr->msg_stream_id = src_pkt_ptr->msg_stream_id;
	dst_pkt_ptr->timestamp = src_pkt_ptr->timestamp;
	// No copy following...
	//dst_pkt_ptr->size = src_pkt_ptr->size;
	//dst_pkt_ptr->valid = src_pkt_ptr->valid;
	//dst_pkt_ptr->data_ptr = src_pkt_ptr->data_ptr;
}

static inline uint32_t gettime()
{
#ifdef _WIN32
	return timeGetTime();
#else
	tms t;
	return (times(&t) * 1000 / sysconf(_SC_CLK_TCK));
#endif
}

