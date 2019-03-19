#pragma once
#include "common/rt_status.h"
#include "common/rt_defines.h"
#include "amf.h"


/**
* @brief:
* RTMP client
*/
class CRTMPClient
{
public:
	CRTMPClient();
	virtual ~CRTMPClient();

	rt_status_t create(const char *url);
	void destroy();
	rt_status_t connect(uint32_t timeout_secs);
	void disconnect();

protected:
	rt_status_t _init_network();
	void _deinit_network();
	rt_status_t _parse_url(const char *url);
	rt_status_t _handshake();
	rt_status_t _invoke_connect();

	bool _send(uint32_t size, const uint8_t *data_ptr);
	bool _recv(uint32_t size, uint8_t *data_ptr);
	rt_status_t _send_packet(rtmp_packet_t *pkt_ptr, bool queue);

protected:
	rtmp_context_t _context;
};


