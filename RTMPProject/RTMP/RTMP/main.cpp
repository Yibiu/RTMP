#include "RTMPClient.h"


int main()
{
	CRTMPClient rtmp_client;

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
	do {
		if (!rtmp_client.create("E:/TEST/Videos/11.mp4", metadata, STREAM_FILE_MP4)) {
			printf("RTMP create error!\n");
			break;
		}
		if (!rtmp_client.connect("rtmp://192.168.0.100:1935/live/stream", 5)) {
			printf("RTMP connect error!\n");
			break;
		}

		printf("ENTER to exit...\n");
		getchar();
	} while (false);
	rtmp_client.disconnect();
	rtmp_client.destroy();

	return 0;
}

