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
#include "rtmp_client.h"
#include "statistics.h"


typedef enum _stream_type
{
	STREAM_FILE_MP4 = 0,
	STREAM_H264_RAW
} stream_type_t;

typedef struct _fmt_manage
{
	AVFormatContext *fmt_context;
	AVStream *video_stream;
	AVStream *audio_stream;
} fmt_manage_t;


/**
* @brief:
* Test rtmp client
*/
class CTestClient
{
public:
	CTestClient();
	virtual ~CTestClient();

	bool create(const char *path_ptr, rtmp_metadata_t &metadata, stream_type_t type);
	void destroy();
	bool connect(const char *url_ptr, uint32_t timeout_secs);
	void disconnect();

	static void thread_proc(void *param)
	{
		CTestClient *this_ptr = (CTestClient *)param;
		if (NULL != this_ptr)
			this_ptr->thread_proc_internal();
	}
	void thread_proc_internal();

protected:
	bool _parse_streams(rtmp_metadata_t &metadata, stream_type_t type);

protected:
	CRTMPClient *_rtmp_ptr;
	fmt_manage_t _manage;
	rtmp_metadata_t	_metadata;
	bool _running;
	std::thread *_thread_ptr;
};

