#include "rtmp_client.h"


CRTMPClient::CRTMPClient()
{
}

CRTMPClient::~CRTMPClient()
{
}

rt_status_t CRTMPClient::create(const char *url)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	do {
		status = _parse_url(url);
		CHECK_BREAK(rt_is_success(status));
		if (RTMP_PROTOCOL_RTMP != _context.protocol) {
			status = RT_STATUS_NOT_SUPPORT;
			break;
		}

		status = _init_network();
		CHECK_BREAK(rt_is_success(status));
	} while (false);

	if (!rt_is_success(status)) {
		destroy();
	}

	return status;
}

void CRTMPClient::destroy()
{
	_deinit_network();
}

rt_status_t CRTMPClient::connect(uint32_t timeout_secs)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	do {
		_context.socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (-1 == _context.socket) {
			status = RT_STATUS_SOCKET_ERR;
			break;
		}

		// Connect timeout
		sockaddr_in service;
		memset(&service, 0x00, sizeof(sockaddr_in));
		service.sin_family = AF_INET;
		service.sin_addr.s_addr = inet_addr(_context.host.c_str());
		if (INADDR_NONE == service.sin_addr.s_addr) {
			hostent *host = gethostbyname(_context.host.c_str());
			if (NULL == host || NULL == host->h_addr) {
				status = RT_STATUS_INVALID_PARAMETER;
				break;
			}
			service.sin_addr = *(struct in_addr *)host->h_addr;
		}
		service.sin_port = htons(_context.port);
		if (::connect(_context.socket, (sockaddr *)&service, sizeof(sockaddr)) < 0) {
			status = RT_STATUS_SOCKET_ERR;
			break;
		}

		// Rcv and send timeout
#ifdef WIN32
		int tv = timeout_secs * 1000;
#else
		timeval tv = { timeout_secs, 0 };
#endif
		if (0 != setsockopt(_context.socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv))) {
			status = RT_STATUS_SOCKET_ERR;
			break;
		}
		if (setsockopt(_context.socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv))) {
			status = RT_STATUS_SOCKET_ERR;
			break;
		}
		int on = 1;
		setsockopt(_context.socket, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on));

		// Simple handshake
		status = _handshake();
		CHECK_BREAK(rt_is_success(status));
		status = _send_conn_packet();
		CHECK_BREAK(rt_is_success(status));

	} while (false);

	return status;
}

void CRTMPClient::disconnect()
{

}


rt_status_t CRTMPClient::_init_network()
{
	rt_status_t status = RT_STATUS_SUCCESS;

	WORD version = MAKEWORD(2, 2);
	WSADATA wsaData;
	if (0 != WSAStartup(version, &wsaData)) {
		status = RT_STATUS_NETWORK_SETUP;
	}

	return status;
}

void CRTMPClient::_deinit_network()
{
	WSACleanup();
}

rt_status_t CRTMPClient::_parse_url(const char *url)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	std::string url_str = url;
	do {
		if (url_str.empty()) {
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}

		// Parse protocol
		size_t pos = url_str.find("://");
		if (pos == std::string::npos) {
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}
		std::string proto_str = url_str.substr(0, pos);
		if (0 == proto_str.compare("rtmp")) {
			_context.protocol = RTMP_PROTOCOL_RTMP;
		}
		else if (0 == proto_str.compare("rtmpt")) {
			_context.protocol = RTMP_PROTOCOL_RTMPT;
		}
		else if (0 == proto_str.compare("rtmpe")) {
			_context.protocol = RTMP_PROTOCOL_RTMPE;
		}
		else {
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}

		// Parse host and port
		std::string rest_str = url_str.substr(pos + 3);
		if (rest_str.empty()) {
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}
		pos = rest_str.find("/");
		if (pos == std::string::npos) {
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}
		std::string host_str = rest_str.substr(0, pos);
		rest_str = rest_str.substr(pos + 1);
		if (host_str.empty() || rest_str.empty()) {
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}
		pos = host_str.find(":");
		if (pos == std::string::npos) {
			_context.host = host_str;
			_context.port = RTMP_DEFAULT_PORT;
		}
		else {
			_context.host = host_str.substr(0, pos);
			std::string port_str = host_str.substr(pos + 1);
			if (port_str.empty()) {
				_context.port = RTMP_DEFAULT_PORT;
			}
			else {
				_context.port = std::stoi(port_str);
			}
		}

		// Parse application and stream
		pos = rest_str.find_last_of("/");
		if (pos == std::string::npos) {
			_context.app = rest_str;
			_context.stream = "";
		}
		else {
			_context.app = rest_str.substr(0, pos);
			_context.stream = rest_str.substr(pos + 1);
		}
	} while (false);

	return status;
}

rt_status_t CRTMPClient::_handshake()
{
	rt_status_t status = RT_STATUS_SUCCESS;

	do {
		// C0 C1 ---->
		uint8_t C0C1[RTMP_HANDSHAKE_SIG_SIZE + 1];
		C0C1[0] = RTMP_VERSION;
		uint32_t time = htonl(gettime());
		memcpy(C0C1 + 1, &time, 4);
		memset(C0C1 + 5, 0x00, 4);
		for (int i = 9; i < RTMP_HANDSHAKE_SIG_SIZE; i++) {
			C0C1[i] = (uint8_t)(rand() % 256);
		}
		if (!_send(RTMP_HANDSHAKE_SIG_SIZE + 1, C0C1)) {
			status = RT_STATUS_SOCKET_SEND;
			break;
		}
		// <---- S0
		uint8_t S0;
		if (!_recv(1, &S0)) {
			status = RT_STATUS_SOCKET_RECV;
			break;
		}
		if (C0C1[0] != S0) {
			status = RT_STATUS_NOT_SUPPORT;
			break;
		}
		// <---- S1
		uint8_t S1[RTMP_HANDSHAKE_SIG_SIZE];
		if (!_recv(RTMP_HANDSHAKE_SIG_SIZE, S1)) {
			status = RT_STATUS_SOCKET_RECV;
			break;
		}
		// C2(echo to S1) ---->
		if (!_send(RTMP_HANDSHAKE_SIG_SIZE, S1)) {
			status = RT_STATUS_SOCKET_SEND;
			break;
		}
		// <---- S2(echo to C1)
		if (_recv(RTMP_HANDSHAKE_SIG_SIZE, S1)) {
			status = RT_STATUS_SOCKET_RECV;
			break;
		}
		if (0 != memcmp(S1, C0C1 + 1, RTMP_HANDSHAKE_SIG_SIZE)) {
			status = RT_STATUS_NOT_SUPPORT;
			break;
		}
	} while (false);

	return status;
}

rt_status_t CRTMPClient::_invoke_connect()
{
	rt_status_t status = RT_STATUS_SUCCESS;

	/*
	uint8_t buf[RTMP_TEMP_BUFFER_SIZE];
	rt_packet_t packet;
	packet.chunk_type = RT_CHUNK_TYPE0;
	packet.chunk_stream_id = RT_CHUNK_STREAM_OVER_CONNECTION;
	packet.timestamp = 0;
	packet.msg_type = RT_MSG_TYPE_INVOKE;
	packet.msg_stream_id = 0;
	packet.size = RTMP_TEMP_BUFFER_SIZE;
	packet.valid = 0;
	packet.data_ptr = buf + RTMP_RESERVED_HEADER_SIZE;

	uint8_t *ptr = packet.data_ptr;
	ptr = amf_encodestr(ptr, RTMP_AVC_CONNECT);
	ptr = amf_encodenum(ptr, ++_context_ptr->invokes_count);
	*ptr++ = AMF_OBJECT;
	ptr = amf_encode_namedstr(ptr, RTMP_AVC_APP, _context_ptr->link.app.c_str());
	if (_context_ptr->push_mode) {
		ptr = amf_encode_namedstr(ptr, RTMP_AVC_TYPE, RTMP_AVC_NONPRIVATE);
	}
	if (!_context_ptr->link.flashver.empty()) {
		ptr = amf_encode_namedstr(ptr, RTMP_AVC_FLASHVER, _context_ptr->link.flashver.c_str());
	}
	if (!_context_ptr->link.swfurl.empty()) {
		ptr = amf_encode_namedstr(ptr, RTMP_AVC_SWFURL, _context_ptr->link.swfurl.c_str());
	}
	if (!_context_ptr->link.tcurl.empty()) {
		ptr = amf_encode_namedstr(ptr, RTMP_AVC_TCURL, _context_ptr->link.tcurl.c_str());
	}
	if (!_context_ptr->push_mode) {
		ptr = amf_encode_namedbool(ptr, RTMP_AVC_FPAD, false);
		ptr = amf_encode_namednum(ptr, RTMP_AVC_CAPABILITIES, 15.0);
		ptr = amf_encode_namednum(ptr, RTMP_AVC_AUDIOCODECS, 3191.0);
		ptr = amf_encode_namednum(ptr, RTMP_AVC_VIDEOCODECS, 252.0);
		ptr = amf_encode_namednum(ptr, RTMP_AVC_VIDEOFUNCTION, 1.0);
		if (!_context_ptr->link.pageurl.empty()) {
			ptr = amf_encode_namedstr(ptr, RTMP_AVC_PAGEURL, _context_ptr->link.pageurl.c_str());
		}
	}
	*ptr++ = 0x00;
	*ptr++ = 0x00;
	*ptr++ = AMF_OBJECT_END;

	if (!_context_ptr->link.auth.empty()) {
		ptr = amf_encodebool(ptr, true);
		ptr = amf_encodestr(ptr, _context_ptr->link.auth.c_str());
	}
	packet.valid = ptr - packet.data_ptr;
	return _send_packet(&packet);
	*/

	return status;
}

bool CRTMPClient::_send(uint32_t size, const uint8_t *data_ptr)
{
	if (-1 == _context.socket || (size > 0 && NULL == data_ptr))
		return false;

	if (0 == size)
		return true;

	const uint8_t *ptr = data_ptr;
	while (size > 0)
	{
		int ret = send(_context.socket, (char *)ptr, size, 0);
		if (ret < 0) {
			int err = SOCK_ERROR();
			if (EINTR == err)
				continue;
			break;
		}
		else if (0 == ret) {
			break;
		}
		else {
			size -= ret;
			ptr += ret;
			_context.bytes_out += ret;
		}
	}

	return (0 == size);
}

bool CRTMPClient::_recv(uint32_t size, uint8_t *data_ptr)
{
	if (-1 == _context.socket || (size > 0 && NULL == data_ptr))
		return false;

	if (0 == size)
		return true;

	uint8_t *ptr = data_ptr;
	while (size > 0)
	{
		int ret = recv(_context.socket, (char *)ptr, size, 0);
		if (ret < 0) {
			int err = SOCK_ERROR();
			if (EINTR == err)
				continue;
			break;
		}
		else if (0 == ret) {
			break;
		}
		else {
			size -= ret;
			ptr += ret;
			_context.bytes_in += ret;
		}
	}

	return (0 == size);
}

rt_status_t CRTMPClient::_send_packet(rtmp_packet_t *pkt_ptr, bool queue)
{
	/**
	* Chunk Header =
	* Basic Header(1/2/3) + Msg Header(0/3/7/11) + Extend Timestamp(0/4)
	*/
	rt_status_t status = RT_STATUS_SUCCESS;

	do {
		if (NULL == pkt_ptr) {
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}

		// TODO:
		// Packet optimization to support RTMP_CHUNK_TYPE_MEDIUM/
		// /RTMP_CHUNK_TYPE_SMALL/RTMP_CHUNK_TYPE_MINIMUM
		// ...

		////////////////////////////////////////////////
		// Reserve header size
		uint32_t reserved = 0;
		uint8_t fmt = 0;
		uint32_t fmt_extend = 0;
		switch (pkt_ptr->chk_type)
		{
		case RTMP_CHUNK_TYPE_LARGE:
			reserved = 1 + 11;
			fmt = (0x00 << 6);
			break;
		case RTMP_CHUNK_TYPE_MEDIUM:
			reserved = 1 + 7;
			fmt = (0x01 << 6);
			break;
		case RTMP_CHUNK_TYPE_SMALL:
			reserved = 1 + 3;
			fmt = (0x02 << 6);
			break;
		case RTMP_CHUNK_TYPE_MINIMUM:
			reserved = 1 + 0;
			fmt = (0x03 << 6);
			break;
		default:
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}
		CHECK_BREAK(rt_is_success(status));
		// 319~:3 bytes
		// 64~318: 2 bytes
		// 0~63: 1 bytes
		if (pkt_ptr->chk_stream_id > 319) {
			reserved += 2;
			fmt_extend = 2;
		}
		else if (pkt_ptr->chk_stream_id > 63) {
			reserved += 1;
			fmt_extend = 1;
		}
		// Extend timestamp
		if (pkt_ptr->timestamp >= 0xFFFFFF) {
			reserved += 4;
		}

		////////////////////////////////////////////////
		// Fill header buffer
		uint8_t *header_ptr = pkt_ptr->data_ptr - reserved;
		uint8_t *ptr = header_ptr;
		switch (fmt_extend)
		{
		case 0:
			fmt |= pkt_ptr->chk_stream_id;
			*ptr++ = fmt;
			break;
		case 1:
			*ptr++ = fmt;
			*ptr++ = ((pkt_ptr->chk_stream_id - 64) & 0xff);
			break;
		case 2:
			fmt |= 0x01;
			*ptr++ = fmt;
			*ptr++ = ((pkt_ptr->chk_stream_id - 64) & 0xff);
			*ptr++ = (((pkt_ptr->chk_stream_id - 64) >> 8) & 0xff);
			break;
		default:
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}
		CHECK_BREAK(rt_is_success(status));

		if (RTMP_CHUNK_TYPE_MINIMUM != pkt_ptr->chk_type) {
			//ptr = amf_encode_int24(ptr, pkt_ptr->timestamp >= 0xFFFFFF ? 0xFFFFFF : pkt_ptr->timestamp);
		}
		if (RTMP_CHUNK_TYPE_MINIMUM != pkt_ptr->chk_type
			&& RTMP_CHUNK_TYPE_SMALL != pkt_ptr->chk_type) {
			//ptr = amf_encodeu24(ptr, packet_ptr->valid);
			*ptr++ = pkt_ptr->msg_type;
		}
		if (RTMP_CHUNK_TYPE_LARGE == pkt_ptr->chk_type) {
			//ptr = amf_encodeu32(ptr, packet_ptr->msg_stream_id);
		}
		if (pkt_ptr->timestamp >= 0xFFFFFF) {
			//ptr = amf_encodeu32(ptr, t);
		}

		////////////////////////////////////////////////
		// Send packet
		uint32_t chunk_size = _context.out_chunk_size;
		uint32_t data_size = pkt_ptr->valid;
		uint8_t *data_ptr = pkt_ptr->data_ptr;
		while (data_size + reserved > 0)
		{
			if (data_size < chunk_size)
				chunk_size = data_size;

			if (!_send(chunk_size + reserved, header_ptr)) {
				status = RT_STATUS_SOCKET_SEND;
				break;
			}
			data_ptr += chunk_size;
			data_size -= chunk_size;

			// Fill header buffer of splitted frames(without Msg Header)
			// Force chunk type = RTMP_CHUNK_TYPE_MINIMUM
			reserved = 0;
			if (data_size > 0) {
				reserved = 1 + fmt_extend;
				if (pkt_ptr->timestamp >= 0xFFFFFF) {
					reserved += 4;
				}
				header_ptr = data_ptr - reserved;
				header_ptr[0] = (fmt | 0xc0);
				if (1 == fmt_extend) {
					header_ptr[1] = ((pkt_ptr->chk_stream_id - 64) & 0xff);
				}
				else if (2 == fmt_extend) {
					header_ptr[1] = ((pkt_ptr->chk_stream_id - 64) & 0xff);
					header_ptr[2] = (((pkt_ptr->chk_stream_id - 64) >> 8) & 0xff);
				}
				if (pkt_ptr->timestamp >= 0xFFFFFF) {
					//amf_encodeu32(header_ptr + 1 + fmt_extend, t);
				}
			}
		}
		CHECK_BREAK(rt_is_success(status));

		////////////////////////////////////////////////
		// Store for watting for results
		// Invoke: xInvokeFunction...
		if (RTMP_MSG_TYPE_INVOKE == pkt_ptr->msg_type) {
			val_t invoke;
			char *ptr = (char *)pkt_ptr->data_ptr + 1;
			//amf_decode_string(ptr, invoke);

			if (queue) {
				ptr += invoke.len + 3;
				//int txn = amf_decode_number(ptr);

			}
		}

	} while (false);

	return status;
}


