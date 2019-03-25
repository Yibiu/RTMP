#include <stdio.h>
#include <stdint.h>
#include "rtmp_client.h"
#include "common/logger.h"


int main(int argc, char *argv[])
{
	rt_status_t status = RT_STATUS_SUCCESS;
	CRTMPClient client;
	
	do {
		CLogger::get_instance()->add_level(CLogger::LOG_LEVEL_DEBUG);
		CLogger::get_instance()->add_level(CLogger::LOG_LEVEL_WARNING);
		CLogger::get_instance()->add_level(CLogger::LOG_LEVEL_ERROR);
		CLogger::get_instance()->add_tag(TAG_AMF);
		CLogger::get_instance()->add_tag(TAG_RTMP);
		CLogger::get_instance()->open_console();

		status = client.create("rtmp://192.168.0.100:1935/live/stream");
		if (!rt_is_success(status))
			break;

		status = client.connect(3);
		if (!rt_is_success(status))
			break;
	} while (false);
	client.disconnect();
	client.destroy();
	//CLogger::get_instance()->close_console();

	return 0;
}


