#pragma once
#include <vector>
#include <list>
#include <map>
#include "rt_status.h"
#include "rt_defines.h"
#include "amf.h"


class CRTMPClient
{
public:
	CRTMPClient();
	virtual ~CRTMPClient();

	rt_status_t create(const char *url);
	void destroy();
	rt_status_t connect(uint32_t timeout_secs, bool retry = false);
	void disconnect();

protected:
	rt_status_t _init_network();
	void _deinit_network();
	rt_status_t _parse_url(const char *url);
	rt_status_t _handshake();
	rt_status_t _send_conn_packet();
	rt_status_t _connect_stream();

	rt_status_t _send_packet(rt_packet_t *packet_ptr);
	bool _send(uint32_t size, const uint8_t *data_ptr);
	bool _recv(uint32_t size, uint8_t *data_ptr);
	int _recv_buffer();

protected:
	rt_context_t *_context_ptr;

	map<uint32_t, rt_packet_t> _last_packets;
};

