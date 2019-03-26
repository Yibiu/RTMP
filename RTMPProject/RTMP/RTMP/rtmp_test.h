#pragma once
#pragma warning( disable:4996)
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <thread>
extern "C"
{
#include "libavformat/avformat.h"
#include "libavutil/time.h"
}
#include "../LibRTMP/rtmp.h"
#include "../LibRTMP/log.h"
#include "statistics.h"


//#define LOG_FILE	1

#define RTMP_METADATA_SIZE			1024
#define RTMP_RESERVED_HEAD_SIZE		9


typedef enum _stream_type
{
	STREAM_FILE_MP4 = 0,
	STREAM_H264_RAW
} stream_type_t;

#define H264_PARAM_LEN		128
typedef struct _h264_param
{
	uint32_t size_sps;
	uint32_t size_pps;
	uint8_t	data_sps[H264_PARAM_LEN];
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

typedef struct _fmt_manage
{
	AVFormatContext *fmt_context;
	AVStream *video_stream;
	AVStream *audio_stream;
} fmt_manage_t;


/**
* @brief:
* Test class for LibRTMP.
*
* Read frames from file --> dispatch by fps --> rtmp.
* Only video frames. The pts read from file may be 0.
*/
class CTestPusher
{
public:
	CTestPusher();
	virtual ~CTestPusher();

	bool create(const char *path_ptr, rtmp_metadata_t &metadata, stream_type_t type);
	void destroy();
	bool connect(const char *url_ptr, uint32_t timeout_secs);
	void disconnect();

	static void thread_proc(void *param)
	{
		CTestPusher *this_ptr = (CTestPusher *)param;
		if (NULL != this_ptr)
			this_ptr->thread_proc_internal();
	}
	void thread_proc_internal();

protected:
	bool _init_sockets();
	void _cleanup_sockets();
	bool _parse_streams(rtmp_metadata_t &metadata, stream_type_t type);
	bool _send_metadata(const rtmp_metadata_t &metadata);
	bool _send_video(uint32_t size, const uint8_t *data_ptr, uint64_t pts, bool keyframe);
	bool _send_audio(uint32_t size, const uint8_t *data_ptr, uint64_t pts);

protected:
	RTMP *_rtmp_ptr;
	fmt_manage_t _manage;
	rtmp_metadata_t	_metadata;
	bool _running;
	std::thread *_thread_ptr;

#ifdef LOG_FILE
	FILE *_file_ptr;
#endif
};


/**
* @brief:
* Test class for rtmp puller
* 
* Frames from server --> puller --> local file(.flv or .h264 or .aac)
*/
class CTestPuller
{
public:
	CTestPuller();
	virtual ~CTestPuller();

	bool create(const char *path_ptr);
	void destroy();
	bool connect(const char *url_ptr, uint32_t timeout_secs);
	void disconnect();

	static void thread_proc(void *param)
	{
		CTestPuller *this_ptr = (CTestPuller *)param;
		if (NULL != this_ptr)
			this_ptr->thread_proc_internal();
	}
	void thread_proc_internal();

protected:
	bool _init_sockets();
	void _cleanup_sockets();

protected:
	RTMP *_rtmp_ptr;
	bool _running;
	std::thread *_thread_ptr;

	uint32_t _buffer_size;
	uint8_t *_buffer_ptr;
	FILE *_file_ptr;
};

