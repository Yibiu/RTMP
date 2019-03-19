#pragma once


#define RTMP_TEMP_BUFFER_SIZE			4096
//
#define RTMP_RESERVED_HEADER_SIZE		18

//
#define RTMP_DEFAULT_PORT				1935

#define RTMP_CONN_TIMEOUT_SECS			10
#define RTMP_CONN_RETRY_TIMES			3
#define RTMP_CONN_RETRY_PERIOD_SECS		20


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

