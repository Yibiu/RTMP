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
	rt_status_t _invoke_release_stream();
	rt_status_t _invoke_fcpublish();
	rt_status_t _invoke_create_stream();

	rt_status_t _handle_chunk_size(rtmp_packet_t *pkt_ptr);
	rt_status_t _handle_bytes_read_report(rtmp_packet_t *pkt_ptr);
	rt_status_t _handle_control(rtmp_packet_t *pkt_ptr);
	rt_status_t _handle_server_bw(rtmp_packet_t *pkt_ptr);
	rt_status_t _handle_client_bw(rtmp_packet_t *pkt_ptr);
	rt_status_t _handle_audio(rtmp_packet_t *pkt_ptr);
	rt_status_t _handle_video(rtmp_packet_t *pkt_ptr);
	rt_status_t _handle_flex_stream_send(rtmp_packet_t *pkt_ptr);
	rt_status_t _handle_flex_shared_object(rtmp_packet_t *pkt_ptr);
	rt_status_t _handle_flex_message(rtmp_packet_t *pkt_ptr);
	rt_status_t _handle_info(rtmp_packet_t *pkt_ptr);
	rt_status_t _handle_shared_object(rtmp_packet_t *pkt_ptr);
	rt_status_t _handle_invoke(rtmp_packet_t *pkt_ptr);
	rt_status_t _handle_flash_video(rtmp_packet_t *pkt_ptr);
	rt_status_t _handle_packet(rtmp_packet_t *pkt_ptr);

	bool _send(uint32_t size, const uint8_t *data_ptr);
	bool _recv(uint32_t size, uint8_t *data_ptr);
	rt_status_t _send_packet(rtmp_packet_t *pkt_ptr, bool queue);
	rt_status_t _recv_packet(rtmp_packet_t *pkt_ptr);

protected:
	rtmp_context_t _context;

	rtmp_packet_t *_recv_pkt_ptr;
};


