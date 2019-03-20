#include "rtmp_client.h"


#define AVDEF(x)	static const val_t av_##x = AVINIT(#x)
AVDEF(connect);
AVDEF(app);
AVDEF(type);
AVDEF(nonprivate);
AVDEF(flashVer);
AVDEF(swfUrl);
AVDEF(tcUrl);
AVDEF(objectEncoding);


uint32_t g_msg_header_size[4] = { 11, 7, 3, 0 };


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
		if (RTMP_PROTOCOL_RTMP != _context.link.protocol) {
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
		service.sin_addr.s_addr = inet_addr(_context.link.host.c_str());
		if (INADDR_NONE == service.sin_addr.s_addr) {
			hostent *host = gethostbyname(_context.link.host.c_str());
			if (NULL == host || NULL == host->h_addr) {
				status = RT_STATUS_INVALID_PARAMETER;
				break;
			}
			service.sin_addr = *(struct in_addr *)host->h_addr;
		}
		service.sin_port = htons(_context.link.port);
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

		// "connect" ---->
		status = _invoke_connect();
		CHECK_BREAK(rt_is_success(status));

		// <---->
		while (true)
		{
			// TODO
			// ...
		}
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
			_context.link.protocol = RTMP_PROTOCOL_RTMP;
		}
		else if (0 == proto_str.compare("rtmpt")) {
			_context.link.protocol = RTMP_PROTOCOL_RTMPT;
		}
		else if (0 == proto_str.compare("rtmpe")) {
			_context.link.protocol = RTMP_PROTOCOL_RTMPE;
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
			_context.link.host = host_str;
			_context.link.port = RTMP_DEFAULT_PORT;
		}
		else {
			_context.link.host = host_str.substr(0, pos);
			std::string port_str = host_str.substr(pos + 1);
			if (port_str.empty()) {
				_context.link.port = RTMP_DEFAULT_PORT;
			}
			else {
				_context.link.port = std::stoi(port_str);
			}
		}

		// Parse application and stream
		pos = rest_str.find_last_of("/");
		if (pos == std::string::npos) {
			_context.link.app = rest_str;
			_context.link.stream = "";
		}
		else {
			_context.link.app = rest_str.substr(0, pos);
			_context.link.stream = rest_str.substr(pos + 1);
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

	rtmp_packet_t packet;
	packet.msg_type = RTMP_MSG_TYPE_INVOKE;
	packet.msg_stream_id = 0; // Because of no stream now
	packet.chk_type = RTMP_CHUNK_TYPE_LARGE;
	packet.chk_stream_id = RTMP_CHUNK_STREAM_OVER_CONNECTION;
	packet.timestamp = 0;

	//
	// Push:
	// connect + invokes + app + type + flashVer + swfUrl + tcUrl
	// + objectEncoding + auth + extras
	// Pull:
	// connect + invokes + app + flashVer + swfUrl + tcUrl
	// + fpad + capabilities + audioCodecs + videoCodecs + videoFunction + pageUrl
	// + objectEncoding + auth + extras
	//
	uint8_t buffer[4096];
	packet.data_ptr = buffer + RTMP_MAX_HEADER_SIZE; // Reserved to fill header
	uint8_t *ptr = packet.data_ptr;
	ptr = amf_encode_string(ptr, av_connect);
	ptr = amf_encode_number(ptr, ++_context.num_invokes);
	*ptr++ = AMF_OBJECT;
	val_t app = { (char *)_context.link.app.c_str(), _context.link.app.length() };
	ptr = amf_encode_named_string(ptr, av_app, app);
	ptr = amf_encode_named_string(ptr, av_type, av_nonprivate);
	if (!_context.params.flashVer.empty()) {
		val_t flashVer = { (char *)_context.params.flashVer.c_str(), _context.params.flashVer.length() };
		ptr = amf_encode_named_string(ptr, av_flashVer, flashVer);
	}
	if (!_context.params.swfUrl.empty()) {
		val_t swfUrl = { (char *)_context.params.swfUrl.c_str(), _context.params.swfUrl.length() };
		ptr = amf_encode_named_string(ptr, av_swfUrl, swfUrl);
	}
	if (!_context.params.tcUrl.empty()) {
		val_t tcUrl = { (char *)_context.params.tcUrl.c_str(), _context.params.tcUrl.length() };
		ptr = amf_encode_named_string(ptr, av_tcUrl, tcUrl);
	}
	if (0 != _context.params.encoding) {
		// AMF3
		ptr = amf_encode_named_number(ptr, av_objectEncoding, _context.params.encoding);
	}
	*ptr++ = 0x00;
	*ptr++ = 0x00;
	*ptr++ = AMF_OBJECT_END; // end of object - 0x00 0x00 0x09
	if (!_context.params.auth.empty()) {
		val_t auth = { (char *)_context.params.auth.c_str(), _context.params.auth.length() };
		ptr = amf_encode_boolean(ptr, true);
		ptr = amf_encode_string(ptr, auth);
	}
	//if (_context.extras.num > 0) {
	//	for (int i = 0; i < _context.extras.num; i++) {
	//		ptr = amprop_encode(_context.extras.props[i], ptr);
	//	}
	//}
	packet.size = 4096;
	packet.valid = ptr - packet.data_ptr;

	return _send_packet(&packet, true);
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
			_context.out_bytes_count += ret;
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
			_context.in_bytes_count += ret;
		}
	}

	return (0 == size);
}

rt_status_t CRTMPClient::_send_packet(rtmp_packet_t *pkt_ptr, bool queue)
{
	//
	// Only support RTMP_CHUNK_TYPE_LARGE/RTMP_CHUNK_TYPE_MEDIUM for public.
	// Packet with absolute timestamp.
	// Header size is reserved by data_ptr of packet.
	//
	// Chunk Header = Basic Header(1/2/3) + Msg Header(0/3/7/11) + Extend Timestamp(0/4)
	//
	rt_status_t status = RT_STATUS_SUCCESS;

	do {
		if (NULL == pkt_ptr) {
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}

		uint32_t last_timestamp = 0;

		// Optimization with pre-packet when chunk_type != RTMP_CHUNK_TYPE_LARGE
		if (pkt_ptr->chk_type != RTMP_CHUNK_TYPE_LARGE) {
			std::map<uint32_t, rtmp_packet_t>::iterator iter;
			iter = _context.out_channels.find(pkt_ptr->chk_stream_id);
			if (iter != _context.out_channels.end()) {
				rtmp_packet_t pre_packet = iter->second;

				if (pkt_ptr->chk_type == RTMP_CHUNK_TYPE_MEDIUM
					&& pkt_ptr->msg_type == pre_packet.msg_type
					&& pkt_ptr->valid == pre_packet.valid) {
					pkt_ptr->chk_type = RTMP_CHUNK_TYPE_SMALL;
				}
				if (pkt_ptr->chk_type == RTMP_CHUNK_TYPE_SMALL
					&& pkt_ptr->timestamp == pre_packet.timestamp) { // FIXME: timestamp is all absolute
					pkt_ptr->chk_type = RTMP_CHUNK_TYPE_MINIMUM;
				}
				
				last_timestamp = pre_packet.timestamp; // Use delt timestamp
			}
		}
		uint32_t timestamp = pkt_ptr->timestamp - last_timestamp;

		// Reserve header size
		uint32_t reserved = 1 + g_msg_header_size[pkt_ptr->chk_type];
		uint8_t fmt = (pkt_ptr->chk_type << 6);
		uint32_t fmt_extend = 0;
		if (pkt_ptr->chk_stream_id > 319) {
			fmt_extend = 2;
		}
		else if (pkt_ptr->chk_stream_id > 63) {
			fmt_extend = 1;
		}
		reserved += fmt_extend;
		if (timestamp >= 0xFFFFFF) {
			reserved += 4;
		}

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
			ptr = amf_encode_u24(ptr, timestamp >= 0xFFFFFF ? 0xFFFFFF : timestamp);
		}
		if (RTMP_CHUNK_TYPE_MINIMUM != pkt_ptr->chk_type
			&& RTMP_CHUNK_TYPE_SMALL != pkt_ptr->chk_type) {
			ptr = amf_encode_u24(ptr, pkt_ptr->valid);
			*ptr++ = pkt_ptr->msg_type;
		}
		if (RTMP_CHUNK_TYPE_LARGE == pkt_ptr->chk_type) {
			ptr = amf_encode_u32(ptr, pkt_ptr->msg_stream_id);
		}
		if (timestamp >= 0xFFFFFF) {
			ptr = amf_encode_u32(ptr, timestamp);
		}

		// Split packet to chunks and send them all
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
				if (timestamp >= 0xFFFFFF) {
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
				if (timestamp >= 0xFFFFFF) {
					amf_encode_u32(header_ptr + 1 + fmt_extend, timestamp);
				}
			}
		}
		CHECK_BREAK(rt_is_success(status));

		// Store invoke method for watting for results
		if (RTMP_MSG_TYPE_INVOKE == pkt_ptr->msg_type) {
			val_t content;
			uint8_t *ptr = pkt_ptr->data_ptr + 1; // Skip AMF_STRING/AMF_LONG_STRING
			amf_decode_string(ptr, content);

			if (queue) {
				rtmp_invoke_t invoke;

				ptr += content.len + 3;
				invoke.num = amf_decode_number(ptr);
				invoke.invoke = std::string(content.value, content.len);
				_context.invokes.push_back(invoke);
			}
		}

		// Update this packet to pre-packet
		_context.out_channels[pkt_ptr->chk_stream_id] = *pkt_ptr;
	} while (false);

	return status;
}

rt_status_t CRTMPClient::_recv_packet(rtmp_packet_t *pkt_ptr)
{
	//
	// Read chunks and compose to one entire packet
	//
	// Packet = chunk1 + chunk2 + ...
	// 
	rt_status_t status = RT_STATUS_SUCCESS;

	uint8_t buffer[4096];
	uint8_t *ptr = buffer;
	do {
		if (NULL == pkt_ptr) {
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}

		// Read basic header(1/2/3)
		if (!_recv(1, ptr)) {
			status = RT_STATUS_SOCKET_RECV;
			break;
		}
		pkt_ptr->chk_type = (rtmp_chunk_type_t)((ptr[0] & 0xc0) >> 6);
		pkt_ptr->chk_stream_id = (ptr[0] & 0x3f);
		ptr++;
		if (0 == pkt_ptr->chk_stream_id) {
			if (!_recv(1, ptr)) {
				status = RT_STATUS_SOCKET_RECV;
				break;
			}
			pkt_ptr->chk_stream_id = (ptr[0] + 64);
			ptr++;
		}
		else if (1 == pkt_ptr->chk_stream_id) {
			if (!_recv(2, ptr)) {
				status = RT_STATUS_SOCKET_RECV;
				break;
			}
			pkt_ptr->chk_stream_id = (ptr[1] << 8) + ptr[0] + 64;
			ptr += 2;
		}

		// Optimization with pre-packet when chunk_type != RTMP_CHUNK_TYPE_LARGE
		rtmp_chunk_type_t chk_type = pkt_ptr->chk_type;
		if (pkt_ptr->chk_type != RTMP_CHUNK_TYPE_LARGE) {
			std::map<uint32_t, rtmp_packet_t>::iterator iter;
			iter = _context.in_channels.find(pkt_ptr->chk_stream_id);
			if (iter != _context.in_channels.end()) {
				rtmp_packet_t pre_packet = iter->second;

				rtmp_copy_packet(*pkt_ptr, pre_packet);
				pkt_ptr->chk_type = chk_type;
			}
		}
		sadas

		// Read msg header(11/7/3/0)
		uint32_t size = g_msg_header_size[pkt_ptr->chk_type];
		if (!_recv(size, ptr)) {
			status = RT_STATUS_SOCKET_RECV;
			break;
		}
		if (RTMP_CHUNK_TYPE_MINIMUM != pkt_ptr->chk_type) {
			pkt_ptr->timestamp = amf_decode_u24(ptr);
			ptr += 3;
		}
		if (RTMP_CHUNK_TYPE_MINIMUM != pkt_ptr->chk_type
			&& RTMP_CHUNK_TYPE_SMALL != pkt_ptr->chk_type) {
			pkt_ptr->size = amf_decode_u24(ptr);
			ptr += 3;
			pkt_ptr->msg_type = (rtmp_msg_type_t)ptr[0];
			ptr += 1;
		}
		if (RTMP_CHUNK_TYPE_LARGE == pkt_ptr->chk_type) {
			//pkt_ptr->abs_timestamp = true;
			pkt_ptr->msg_stream_id = amf_decode_u32le(ptr);
			ptr += 4;
		}

		// Extend timestamp
		bool extend_timestamp = 0xFFFFFF == pkt_ptr->timestamp;
		if (extend_timestamp) {
			if (!_recv(4, ptr)) {
				status = RT_STATUS_SOCKET_RECV;
				break;
			}
			pkt_ptr->timestamp = amf_decode_u32(ptr);
			ptr += 4;
		}

		// Read one chunk
		uint32_t bytes_to_read = pkt_ptr->size - pkt_ptr->valid;
		uint32_t chunk_size = _context.in_chunk_size;
		if (bytes_to_read < chunk_size)
			chunk_size = bytes_to_read;
		if (!_recv(chunk_size, pkt_ptr->data_ptr)) {
			status = RT_STATUS_SOCKET_RECV;
			break;
		}
		pkt_ptr->valid += chunk_size;

		// TODO
		// Copy reference...
		//

		if (pkt_ptr->size == pkt_ptr->valid) {
		}
		else {
		}
	} while (false);

	return status;
}



