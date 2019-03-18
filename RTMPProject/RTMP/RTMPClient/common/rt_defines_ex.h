#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <string>
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
using namespace std;


#define CHECK_BREAK(x)	if (!x) { break; }
#define CHECK_STATUS(x, status, value) if (!x) {status=value; break;}


#define RTMP_TEMP_BUFFER_SIZE			4096
//
#define RTMP_RESERVED_HEADER_SIZE		18

//
#define RTMP_DEFAULT_PORT				1935

#define RTMP_CONN_TIMEOUT_SECS			10
#define RTMP_CONN_RETRY_TIMES			3
#define RTMP_CONN_RETRY_PERIOD_SECS		20

#define RTMP_HANDSHAKE_SIG_SIZE			1536
#define RTMP_RECV_BUFFER_SIZE			(16 * 1024)
#define RTMP_DEFAULT_CHUNK_SIZE			128

//
#define RTMP_AVC_APP			"app"
#define RTMP_AVC_CONNECT		"connect"
#define RTMP_AVC_FLASHVER		"flashVer"
#define RTMP_AVC_SWFURL			"swfUrl"
#define RTMP_AVC_PAGEURL		"pageUrl"
#define RTMP_AVC_TCURL			"tcUrl"
#define RTMP_AVC_FPAD			"fpad"
#define RTMP_AVC_CAPABILITIES	"capabilities"
#define RTMP_AVC_AUDIOCODECS	"audioCodecs"
#define RTMP_AVC_VIDEOCODECS	"videoCodecs"
#define RTMP_AVC_VIDEOFUNCTION	"videoFunction"
#define RTMP_AVC_OBJENCODING	"objectEncoding"
#define RTMP_AVC_SECURETOKEN	"secureToken"
#define RTMP_AVC_SECURETOKENRSP	"secureTokenResponse"
#define RTMP_AVC_TYPE			"type"
#define RTMP_AVC_NONPRIVATE		"nonprivate"
#define RTMP_AVC_CREATESTREAM	"createStream"
#define RTMP_AVC_FCSUBSCRIBE	"FCSubscribe"
#define RTMP_AVC_RELEASESTREAM	"releaseStream"
#define RTMP_AVC_FCPUBLISH		"FCPublish"
#define RTMP_AVC_UNPUBLISH		"FCUnpublish"
#define RTMP_AVC_PUBLISH		"publish"
#define RTMP_AVC_LIVE			"live"
#define RTMP_AVC_RECORD			"record"
#define RTMP_ACV_DELETESTREAM	"deleteStream"
#define RTMP_AVC_PAUSE			"pause"
#define RTMP_AVC_SEEK			"seek"
#define RTMP_AVC__CHECKBW		"_checkbw"
#define RTMP_AVC__RESULT		"_result"
#define RTMP_AVC_PING			"ping"
#define RTMP_AVC_PONG			"pong"
#define RTMP_AVC_PLAY			"play"
#define RTMP_AVC_SETPLAYLIST	"set_playlist"
#define RTMP_AVC_ONBWDONE		"onBWDone"
#define RTMP_AVC_ONFCSUBSCRIBE	"onFCSubscribe"
#define RTMP_AVC__ONBWCHECK		"_onbwcheck"
#define RTMP_AVC__ONBWDONE		"_onbwdone"
#define RTMP_AVC__ERROR			"_error"
#define RTMP_AVC_CLOSE			"close"
#define RTMP_AVC_CODE			"code"
#define RTMP_AVC_LEVEL			"level"
#define RTMP_AVC_DESCRIPTION	"description"
#define RTMP_AVC_ONSTATUS		"onStatus"
#define RTMP_AVC_PLAYLISTREADY	"playlist_ready"
#define RTMP_AVC_ONMETADATA		"onMetaData"
#define RTMP_AVC_DURATION		"duration"
#define RTMP_AVC_VIDEO			"video"
#define RTMP_AVC_AUDIO			"audio"



typedef enum _rt_msg_type
{
	RT_MSG_TYPE_RESERVED00			= 0x00,
	RT_MSG_TYPE_CHUNK_SIZE			= 0x01,
	RT_MSG_TYPE_RESERVED02			= 0x02,
	RT_MSG_TYPE_BYTES_READ_REPORT	= 0x03,
	RT_MSG_TYPE_CONTROL				= 0x04,
	RT_MSG_TYPE_SERVER_BW			= 0x05,
	RT_MSG_TYPE_CLIENT_BW			= 0x06,
	RT_MSG_TYPE_RESERVED07			= 0x07,
	RT_MSG_TYPE_AUDIO				= 0x08,
	RT_MSG_TYPE_VIDEO				= 0x09,
	RT_MSG_TYPE_RESERVED0A			= 0x0A,
	RT_MSG_TYPE_RESERVED0B			= 0x0B,
	RT_MSG_TYPE_RESERVED0C			= 0x0C,
	RT_MSG_TYPE_RESERVED0D			= 0x0D,
	RT_MSG_TYPE_RESERVED0E			= 0x0E,
	RT_MSG_TYPE_FLEX_STREAM_SEND	= 0x0F,
	RT_MSG_TYPE_FLEX_SHARED_OBJECT	= 0x10,
	RT_MSG_TYPE_FLEX_MESSAGE		= 0x11,
	RT_MSG_TYPE_INFO				= 0x12,
	RT_MSG_TYPE_SHARED_OBJECT		= 0x13,
	RT_MSG_TYPE_INVOKE				= 0x14,
	RT_MSG_TYPE_RESERVED15			= 0x15,
	RT_MSG_TYPE_FLASH_VIDEO			= 0x16
} rt_msg_type;

typedef enum _rt_chunk_type
{
	RT_CHUNK_TYPE0 = 0x00,
	RT_CHUNK_TYPE1,
	RT_CHUNK_TYPE2,
	RT_CHUNK_TYPE3
} rt_chunk_type;

typedef enum _rt_chunk_stream
{
	RT_CHUNK_STREAM_RESERVED00			= 0x00,
	RT_CHUNK_STREAM_RESERVED01			= 0x01,	
	RT_CHUNK_STREAM_PROTOCOL_CONTROL	= 0x02,
	RT_CHUNK_STREAM_OVER_CONNECTION		= 0x03,
	RT_CHUNK_STREAM_OVER_CONNECTION2	= 0x04,
	RT_CHUNK_STREAM_OVER_STREAM			= 0x05,
	RT_CHUNK_STREAM_VIDEO				= 0x06,
	RT_CHUNK_STREAM_AUDIO				= 0x07,
	RT_CHUNK_STREAM_OVER_STREAM2		= 0x08
} rt_chunk_stream_t;



typedef struct _rt_packet
{
	uint8_t chunk_type;
	uint32_t chunk_stream_id;

	bool abs_timestamp;
	uint32_t timestamp;
	uint8_t msg_type;
	uint32_t msg_stream_id;

	uint32_t size;
	uint32_t valid;
	uint8_t *data_ptr;
} rt_packet_t;

typedef struct _rt_link
{
	string protocol;
	string hostname;
	uint32_t port;
	string app;
	string playpath;
	string playpath0;

	string flashver;
	string swfurl;
	string tcurl;
	string auth;
	string pageurl;
} rt_link_t;

typedef struct _rt_sockbuf
{
	int socket;

	uint32_t avail;
	uint8_t *ptr;
	uint8_t buf[RTMP_RECV_BUFFER_SIZE];
} rt_sockbuf_t;

typedef struct _rt_context
{
	bool push_mode;
	uint32_t out_chunk_size;
	uint32_t invokes_count;
	bool playing;

	rt_link_t link;
	rt_sockbuf_t sock;
} rt_context_t;


static inline void rt_packet_defaults(rt_packet_t *packet_ptr)
{
	if (NULL == packet_ptr)
		return;

	// TODO:
	// ...
}

static inline bool rt_packet_new(rt_packet_t *packet_ptr, uint32_t size)
{
	if (NULL == packet_ptr)
		return false;

	uint8_t *data_ptr = new uint8_t[size + RTMP_RESERVED_HEADER_SIZE];
	if (NULL == data_ptr)
		return false;

	packet_ptr->size = size + RTMP_RESERVED_HEADER_SIZE;
	packet_ptr->data_ptr = data_ptr + RTMP_RESERVED_HEADER_SIZE;
	return true;
}

static inline void rt_packet_delete(rt_packet_t *packet_ptr)
{
	if (NULL == packet_ptr)
		return;

	if (NULL != packet_ptr->data_ptr) {
		delete[] packet_ptr->data_ptr;
		packet_ptr->data_ptr = NULL;
	}
	packet_ptr->size = 0;
}

static inline uint32_t rt_gettime()
{
#ifdef WIN32
	return timeGetTime();
#else
	tms t;
	return (times(&t) * 1000 / sysconf(_SC_CLK_TCK));
#endif
}