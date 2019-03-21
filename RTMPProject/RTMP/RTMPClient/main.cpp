#include <stdio.h>
#include <stdint.h>
#include "rtmp_client.h"


int main(int argc, char *argv[])
{
	CRTMPClient client;
	
	rt_status_t status = client.create("rtmp://192.168.0.100:1935/live/stream");
	if (!rt_is_success(status))
		return -1;
	status = client.connect(3);
	if (!rt_is_success(status))
		return -1;

	return 0;
}