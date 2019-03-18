#include "rtmp.h"


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
		if (NULL == url) {
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}
		_context_ptr = new rt_context_t;
		if (NULL == _context_ptr) {
			status = RT_STATUS_MEMORY_ALLOCATE;
			break;
		}
		status = _parse_url(url);
		CHECK_BREAK(rt_is_success(status));
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
	if (NULL != _context_ptr) {
		delete _context_ptr;
		_context_ptr = NULL;
	}
}

rt_status_t CRTMPClient::connect(uint32_t timeout_secs, bool retry)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	do {
		sockaddr_in service;
		memset(&service, 0x00, sizeof(sockaddr_in));
		service.sin_addr.s_addr = inet_addr(_context_ptr->link.hostname.c_str());
		if (INADDR_NONE == service.sin_addr.s_addr) {
			hostent *host = gethostbyname(_context_ptr->link.hostname.c_str());
			if (NULL == host || NULL == host->h_addr) {
				status = RT_STATUS_INVALID_PARAMETER;
				break;
			}
			service.sin_addr = *(struct in_addr *)host->h_addr;
		}
		service.sin_port = htons(_context_ptr->link.port);

		_context_ptr->sock.socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (-1 == _context_ptr->sock.socket) {
			status = RT_STATUS_SOCK_ERROR;
			break;
		}
		if (::connect(_context_ptr->sock.socket, (sockaddr *)&service, sizeof(sockaddr)) < 0) {
			status = RT_STATUS_CONN_ERROR;
			break;
		}
#ifdef WIN32
		int tv = timeout_secs * 1000;
#else
		timeval tv = { timeout_secs, 0 };
#endif
		if (setsockopt(_context_ptr->sock.socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv))) {
			status = RT_STATUS_SOCK_ERROR;
			break;
		}
		if (setsockopt(_context_ptr->sock.socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv))) {
			status = RT_STATUS_SOCK_ERROR;
			break;
		}
		int on = 1;
		setsockopt(_context_ptr->sock.socket, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on));

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


	return status;
}

void CRTMPClient::_deinit_network()
{

}

// "rtmp://hostname[:port]/app[/appinstance][/...]"
// application = app[/appinstance][/...]
rt_status_t CRTMPClient::_parse_url(const char *url)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	string str_url = url;
	size_t pos = string::npos;
	do {
		if (str_url.empty()) {
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}
		// protocol
		size_t pos = str_url.find("://");
		if (string::npos == pos) {
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}
		_context_ptr->link.protocol = str_url.substr(0, pos);
		// hostname and port
		str_url = str_url.substr(pos + 3);
		if (str_url.empty()) {
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}
		string host_and_port = str_url;
		pos = str_url.find("/");
		if (string::npos != pos) {
			host_and_port = str_url.substr(0, pos);
			str_url = str_url.substr(pos + 1);
		}
		pos = host_and_port.find(":");
		if (string::npos == pos) {
			_context_ptr->link.hostname = host_and_port;
			_context_ptr->link.port = RTMP_DEFAULT_PORT;
		}
		else {
			_context_ptr->link.hostname = host_and_port.substr(0, pos);
			_context_ptr->link.port = atoi(host_and_port.substr(pos + 1).c_str());
		}
		// app
		if (str_url.empty()) {
			break;
		}
		pos = str_url.find_last_of("/");
		if (string::npos == pos) {
			_context_ptr->link.app = str_url;
			break;
		}
		else {
			_context_ptr->link.app = str_url.substr(0, pos);
			str_url = str_url.substr(pos + 1);
		}
		// playpath
		_context_ptr->link.playpath = str_url;
	} while (false);

	return status;
}

rt_status_t CRTMPClient::_handshake()
{
	rt_status_t status = RT_STATUS_SUCCESS;

	do {
		// Send C0 C1
		uint8_t C0C1[RTMP_HANDSHAKE_SIG_SIZE + 1];
		C0C1[0] = 0x03;
		uint32_t time = htonl(rt_gettime());
		memcpy(C0C1 + 1, &time, 4);
		memset(C0C1 + 5, 0x00, 4);
		for (int i = 8; i < RTMP_HANDSHAKE_SIG_SIZE; i++) {
			C0C1[i + 1] = (uint8_t)(rand() % 256);
		}
		if (!_send(RTMP_HANDSHAKE_SIG_SIZE + 1, C0C1)) {
			status = RT_STATUS_SEND_ERROR;
			break;
		}
		// Recv S0 S1
		uint8_t S0;
		uint8_t S1[RTMP_HANDSHAKE_SIG_SIZE];
		if (1 != _recv(1, &S0)) {
			status = RT_STATUS_RECV_ERROR;
			break;
		}
		if (C0C1[0] != S0) {
			status = RT_STATUS_MISMATCH;
			break;
		}
		if (RTMP_HANDSHAKE_SIG_SIZE != _recv(RTMP_HANDSHAKE_SIG_SIZE, S1)) {
			status = RT_STATUS_RECV_ERROR;
			break;
		}
		// Send C2(echo to S1)
		if (!_send(RTMP_HANDSHAKE_SIG_SIZE, S1)) {
			status = RT_STATUS_SEND_ERROR;
			break;
		}
		// Recv S2(echo to C1)
		if (RTMP_HANDSHAKE_SIG_SIZE != _recv(RTMP_HANDSHAKE_SIG_SIZE, S1)) {
			status = RT_STATUS_RECV_ERROR;
			break;
		}
		if (0 != memcmp(S1, C0C1 + 1, RTMP_HANDSHAKE_SIG_SIZE)) {
			status = RT_STATUS_MISMATCH;
			break;
		}
	} while (false);

	return status;
}

rt_status_t CRTMPClient::_send_conn_packet()
{
	rt_status_t status = RT_STATUS_SUCCESS;

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
}

rt_status_t CRTMPClient::_connect_stream()
{
	rt_status_t status = RT_STATUS_SUCCESS;


}

// No optimize for CHUNK_TYPE0
rt_status_t CRTMPClient::_send_packet(rt_packet_t *packet_ptr)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	do {
		if (NULL == packet_ptr) {
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}
		uint32_t last_time_delta = 0;
		map<uint32_t, rt_packet_t>::iterator iter;
		iter = _last_packets.find(packet_ptr->chunk_stream_id);
		if (iter != _last_packets.end() && RT_CHUNK_TYPE0 != packet_ptr->chunk_type) {
			rt_packet_t &pre_packet = iter->second;

			// Optimize
			if (RT_CHUNK_TYPE1 == packet_ptr->chunk_type) {
				if (pre_packet.valid == packet_ptr->valid && pre_packet.msg_type == packet_ptr->msg_type) {
					packet_ptr->chunk_type = RT_CHUNK_TYPE2;
				}
			}
			if (RT_CHUNK_TYPE2 == packet_ptr->chunk_type) {
				if (pre_packet.timestamp == packet_ptr->timestamp) {
					packet_ptr->chunk_type = RT_CHUNK_TYPE3;
				}
			}
			
			last_time_delta = pre_packet.timestamp;
		}

		uint32_t t = packet_ptr->timestamp - last_time_delta;

		uint32_t reserved = 0;
		uint8_t fmt = 0;
		uint32_t fmt_extend = 0;
		switch (packet_ptr->chunk_type)
		{
		case RT_CHUNK_TYPE0:
			reserved = 1 + 11;
			fmt = (0x00 << 6);
			break;
		case RT_CHUNK_TYPE1:
			reserved = 1 + 7;
			fmt = (0x01 << 6);
			break;
		case RT_CHUNK_TYPE2:
			reserved = 1 + 3;
			fmt = (0x02 << 6);
			break;
		case RT_CHUNK_TYPE3:
			reserved = 1 + 0;
			fmt = (0x03 << 6);
			break;
		default:
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}
		CHECK_BREAK(rt_is_success(status));
		if (packet_ptr->chunk_stream_id > 319) {
			reserved += 2;
			fmt_extend = 2;
		}
		else if (packet_ptr->chunk_stream_id > 63) {
			reserved += 1;
			fmt_extend = 1;
		}
		if (t >= 0xFFFFFF) {
			reserved += 4;
		}
		// Fill header
		uint8_t *header_ptr = packet_ptr->data_ptr - reserved;
		uint8_t *ptr = header_ptr;
		switch (fmt_extend)
		{
		case 0:
			fmt |= packet_ptr->chunk_stream_id;
			*ptr++ = fmt;
			break;
		case 1:
			*ptr++ = fmt;
			*ptr++ = ((packet_ptr->chunk_stream_id - 64) & 0xff);
			break;
		case 2:
			fmt |= 0x01;
			*ptr++ = fmt;
			*ptr++ = ((packet_ptr->chunk_stream_id - 64) & 0xff);
			*ptr++ = (((packet_ptr->chunk_stream_id - 64) >> 8) & 0xff);
			break;
		default:
			break;
		}
		if (RT_CHUNK_TYPE3 != packet_ptr->chunk_stream_id) {
			ptr = amf_encodeu24(ptr, t > 0xFFFFFF ? 0xFFFFFF : t);
		}
		if (RT_CHUNK_TYPE3 != packet_ptr->chunk_stream_id
			&& RT_CHUNK_TYPE2 != packet_ptr->chunk_stream_id) {
			ptr = amf_encodeu24(ptr, packet_ptr->valid);
			*ptr++ = packet_ptr->msg_type;
		}
		if (RT_CHUNK_TYPE0 == packet_ptr->chunk_stream_id) {
			ptr = amf_encodeu32(ptr, packet_ptr->msg_stream_id);
		}
		if (t >= 0xFFFFFF) {
			ptr = amf_encodeu32(ptr, t);
		}
		// Send packet
		uint8_t *data_ptr = packet_ptr->data_ptr;
		uint32_t chunk_size = _context_ptr->out_chunk_size;
		uint32_t head_size = reserved;
		uint32_t data_size = packet_ptr->valid;
		while (data_size + head_size > 0)
		{
			if (data_size < chunk_size)
				chunk_size = data_size;

			if (!_send(chunk_size + head_size, header_ptr)) {
				status = RT_STATUS_SEND_ERROR;
				break;
			}
			head_size = 0;
			data_ptr += chunk_size;
			data_size -= chunk_size;

			// fill split chunk header to send
			if (data_size > 0) {
				head_size = 1 + fmt_extend;
				header_ptr = data_ptr - head_size;
				if (t >= 0xFFFFFF) {
					head_size += 4;
					header_ptr -= 4;
				}
				header_ptr[0] = (fmt | 0xc0);
				if (1 == fmt_extend) {
					header_ptr[1] = ((packet_ptr->chunk_stream_id - 64) & 0xff);
				}
				else if (2 == fmt_extend) {
					header_ptr[1] = ((packet_ptr->chunk_stream_id - 64) & 0xff);
					header_ptr[2] = (((packet_ptr->chunk_stream_id - 64) >> 8) & 0xff);
				}
				if (t >= 0xFFFFFF) {
					amf_encodeu32(header_ptr + 1 + fmt_extend, t);
				}
			}
		}
		CHECK_BREAK(rt_is_success(status));

		// Invoke a remote method
		if (RT_MSG_TYPE_INVOKE == packet_ptr->msg_type) {
			// TODO
			// ...
		}

		_last_packets[packet_ptr->chunk_stream_id] = *packet_ptr;
	} while (false);

	return status;
}

bool CRTMPClient::_send(uint32_t size, const uint8_t *data_ptr)
{
	if (0 == size)
		return true;

	int ret = 0;
	const uint8_t *ptr = data_ptr;
	while (size > 0)
	{
		ret = send(_context_ptr->sock.socket, (char *)ptr, size, 0);
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
		}
	}

	return (0 == size);
}

bool CRTMPClient::_recv(uint32_t size, uint8_t *data_ptr)
{
	if (0 == size)
		return true;

	bool error = false;
	uint8_t *ptr = data_ptr;
	while (size > 0)
	{
		uint32_t avail = _context_ptr->sock.avail;
		if (0 == avail) {
			if (_recv_buffer() < 1) {
				error = true;
				break;
			}
			avail = _context_ptr->sock.avail;
		}

		uint32_t readable = ((size < avail) ? size : avail);
		memcpy(ptr, _context_ptr->sock.ptr, readable);
		_context_ptr->sock.ptr += readable;
		_context_ptr->sock.avail -= readable;
		// TODO:
		// bytes echo to server...

		size -= readable;
		ptr += readable;
	}

	return (0 == size);
}

int CRTMPClient::_recv_buffer()
{
	if (0 == _context_ptr->sock.avail) {
		_context_ptr->sock.ptr = _context_ptr->sock.buf;
	}

	int ret = 0;
	while (true)
	{
		int to_recv = sizeof(_context_ptr->sock.buf) - (_context_ptr->sock.ptr - _context_ptr->sock.buf)
			- _context_ptr->sock.avail - 1; // Reserved 1 byte for *ptr
		ret = recv(_context_ptr->sock.socket, (char *)_context_ptr->sock.ptr + _context_ptr->sock.avail, to_recv, 0);
		if (-1 == ret) {
			int err = SOCK_ERROR();
			if (EINTR == err)
				continue;
		}
		else {
			_context_ptr->sock.avail += ret;
		}
		break;
	}

	return ret;
}
