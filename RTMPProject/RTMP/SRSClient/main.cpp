#include <stdio.h>
#include "srs_librtmp.h"


void test_pusher()
{
	srs_rtmp_t rtmp = srs_rtmp_create("");

	if (srs_rtmp_handshake(rtmp) != 0) {
		printf("srs_rtmp_handshake error!\n");
		return;
	}
	if (srs_rtmp_connect_app(rtmp) != 0) {
		printf("srs_rtmp_connect_app error!\n");
		return;
	}
	if (srs_rtmp_publish_stream(rtmp) != 0) {
		printf("srs_rtmp_publish_stream error!\n");
		return;
	}

	uint32_t timestamp = 0;
	int size = 4096;
	char *data_ptr = new char[4096];
	if (NULL == data_ptr) {
		printf("Memory allocate error!\n");
		return;
	}
	for (;;) {
		char type = SRS_RTMP_TYPE_VIDEO;
		if (srs_rtmp_write_packet(rtmp, type, timestamp, data_ptr, size) != 0) {
			printf("srs_rtmp_write_packet error!\n");
			break;
		}

		timestamp += 40;
		usleep(40 * 1000);
	}
	delete[] data_ptr;
}

void test_puller()
{
}


int main()
{
	test_pusher();
	//test_puller();

	return 0;
}

