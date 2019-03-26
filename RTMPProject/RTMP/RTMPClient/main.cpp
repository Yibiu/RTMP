#include <stdio.h>
#include <stdint.h>
#include "test_client.h"


void test_pusher()
{
	CTestPusher pusher;

	do {
		CLogger::get_instance()->add_level(CLogger::LOG_LEVEL_DEBUG);
		CLogger::get_instance()->add_level(CLogger::LOG_LEVEL_WARNING);
		CLogger::get_instance()->add_level(CLogger::LOG_LEVEL_ERROR);
		CLogger::get_instance()->add_tag(TAG_AMF);
		CLogger::get_instance()->add_tag(TAG_RTMP);
		//CLogger::get_instance()->open_console();

		rtmp_metadata_t metadata = { 0 };
		metadata.width = 1920;
		metadata.height = 1080;
		metadata.fps = 30;
		metadata.bitrate_kpbs = 1000;
		metadata.has_audio = false;
		metadata.channels = 2;
		metadata.samplerate = 44100;
		metadata.samples_per_frame = 1024;
		metadata.datarate = 64000;
		if (!pusher.create("E:/TEST/Videos/11.mp4", metadata, STREAM_FILE_MP4)) {
			printf("RTMP create error!\n");
			break;
		}
		if (!pusher.connect("rtmp://192.168.0.100:1935/live/stream", 5)) {
			printf("RTMP connect error!\n");
			break;
		}

		printf("ENTER to exit...\n");
		getchar();
	} while (false);
	pusher.disconnect();
	pusher.destroy();
	//CLogger::get_instance()->close_console();
}

void test_puller()
{
	CTestPuller puller;

	do {
		CLogger::get_instance()->add_level(CLogger::LOG_LEVEL_DEBUG);
		CLogger::get_instance()->add_level(CLogger::LOG_LEVEL_WARNING);
		CLogger::get_instance()->add_level(CLogger::LOG_LEVEL_ERROR);
		CLogger::get_instance()->add_tag(TAG_AMF);
		CLogger::get_instance()->add_tag(TAG_RTMP);
		//CLogger::get_instance()->open_console();

		if (!puller.create("./puller.h264")) { // "./puller.flv"
			printf("RTMP create error!\n");
			break;
		}
		if (!puller.connect("rtmp://192.168.0.100:1935/live/stream", 5)) {
			printf("RTMP connect error!\n");
			break;
		}

		printf("ENTER to exit...\n");
		getchar();
	} while (false);
	puller.disconnect();
	puller.destroy();
	//CLogger::get_instance()->close_console();
}


int main(int argc, char *argv[])
{
	//test_pusher();
	test_puller();

	return 0;
}


