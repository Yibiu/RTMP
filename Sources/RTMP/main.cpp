#include <stdio.h>
#include <stdint.h>
#include "rtmp.h"


int main(int argc, char *argv[])
{
	CRTMPClient client;
	
	client.create("rtmp://192.168.1.1:1234/play/path/stream");

	return 0;
}