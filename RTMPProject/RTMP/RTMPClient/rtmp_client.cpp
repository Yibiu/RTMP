#include "rtmp_client.h"


#define AVDEF(x)	static const val_t av_##x = AVINIT(#x)
AVDEF(app);
AVDEF(type);
AVDEF(nonprivate);
AVDEF(flashVer);
AVDEF(swfUrl);
AVDEF(tcUrl);
AVDEF(objectEncoding);
AVDEF(code);
AVDEF(level);
AVDEF(fpad);
AVDEF(capabilities);
AVDEF(audioCodecs);
AVDEF(videoCodecs);
AVDEF(videoFunction);
AVDEF(pageUrl);
static const val_t av_NetStream_Failed = AVINIT("NetStream.Failed");
static const val_t av_NetStream_Play_Failed = AVINIT("NetStream.Play.Failed");
static const val_t av_NetStream_Play_StreamNotFound = AVINIT("NetStream.Play.StreamNotFound");
static const val_t av_NetConnection_Connect_InvalidApp = AVINIT("NetConnection.Connect.InvalidApp");
static const val_t av_NetStream_Play_Start = AVINIT("NetStream.Play.Start");
static const val_t av_NetStream_Play_PublishNotify = AVINIT("NetStream.Play.PublishNotify");
static const val_t av_NetStream_Publish_Start = AVINIT("NetStream.Publish.Start");
static const val_t av_NetStream_Play_Complete = AVINIT("NetStream.Play.Complete");
static const val_t av_NetStream_Play_Stop = AVINIT("NetStream.Play.Stop");
static const val_t av_NetStream_Play_UnpublishNotify = AVINIT("NetStream.Play.UnpublishNotify");
static const val_t av_NetStream_Seek_Notify = AVINIT("NetStream.Seek.Notify");
static const val_t av_NetStream_Pause_Notify = AVINIT("NetStream.Pause.Notify");

// Invoke
AVDEF(connect);
AVDEF(play);
AVDEF(publish);
AVDEF(_checkbw);
AVDEF(set_playlist);
AVDEF(releaseStream);
AVDEF(FCPublish);
AVDEF(createStream);
AVDEF(live);
AVDEF(FCUnpublish);
AVDEF(deleteStream);
AVDEF(FCSubscribe);

// Method
AVDEF(_result);
AVDEF(onBWDone);
AVDEF(onFCSubscribe);
AVDEF(onFCUnsubscribe);
AVDEF(ping);
AVDEF(_onbwcheck);
AVDEF(_onbwdone);
AVDEF(_error);
AVDEF(close);
AVDEF(onStatus);
AVDEF(playlist_ready);


uint32_t g_msg_header_size[4] = { 11, 7, 3, 0 };


CRTMPClient::CRTMPClient()
{
	// Context init
	_context.link.protocol = RTMP_PROTOCOL_RTMP;
	_context.link.host = "";
	_context.link.port = RTMP_DEFAULT_PORT;
	_context.link.app = "";
	_context.link.stream = "";

	_context.params.flashVer = "";
	_context.params.swfUrl = "";
	_context.params.tcUrl = "";
	_context.params.encoding = 0;
	_context.params.auth = "";

	_context.mode = RTMP_MODE_PUSHER;
	_context.buffer_ms = 3600 * 1000;

	_context.socket = -1;
	_context.playing = false;
	_context.stream_id = 0;
	_context.in_chunk_size = RTMP_DEFAULT_CHUNK_SIZE;
	_context.out_chunk_size = RTMP_DEFAULT_CHUNK_SIZE;
	_context.server_bw = RTMP_DEFAULT_BW;
	_context.client_bw = RTMP_DEFAULT_BW;
	_context.client_bw2 = 2;
	_context.in_bytes_count = 0;
	_context.out_bytes_count = 0;

	_context.invoke_ids = 0;
	_context.invokes.clear();
	_context.in_channels.clear();
	_context.out_channels.clear();

	_recv_pkt_ptr = NULL;
}

CRTMPClient::~CRTMPClient()
{
}

rt_status_t CRTMPClient::create(const char *url, rtmp_mode_t mode)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	do {
		status = _init_network();
		CHECK_BREAK(rt_is_success(status));

		status = _parse_url(url);
		CHECK_BREAK(rt_is_success(status));
		if (RTMP_PROTOCOL_RTMP != _context.link.protocol) {
			status = RT_STATUS_NOT_SUPPORT;
			break;
		}
		LogD(TAG_RTMP, "url: %s", url);
		LogD(TAG_RTMP, "protocol: %d", _context.link.protocol);
		LogD(TAG_RTMP, "host: %s", _context.link.host.c_str());
		LogD(TAG_RTMP, "port: %d", _context.link.port);
		LogD(TAG_RTMP, "app: %s", _context.link.app.c_str());
		LogD(TAG_RTMP, "stream: %s", _context.link.stream.c_str());

		_recv_pkt_ptr = new rtmp_packet_t;
		if (NULL == _recv_pkt_ptr) {
			status = RT_STATUS_MEMORY_ALLOCATE;
			break;
		}
		rtmp_init_packet(_recv_pkt_ptr);
		_recv_pkt_ptr->data_ptr = new uint8_t[RTMP_MAX_CHUNK_SIZE];
		if (NULL == _recv_pkt_ptr->data_ptr) {
			status = RT_STATUS_MEMORY_ALLOCATE;
			break;
		}
		_recv_pkt_ptr->size = RTMP_MAX_CHUNK_SIZE;
		_recv_pkt_ptr->valid = 0;

		_context.mode = mode;
	} while (false);

	if (!rt_is_success(status)) {
		destroy();
	}

	return status;
}

void CRTMPClient::destroy()
{
	_deinit_network();

	if (NULL != _recv_pkt_ptr) {
		if (NULL != _recv_pkt_ptr->data_ptr) {
			delete[] _recv_pkt_ptr->data_ptr;
			_recv_pkt_ptr->data_ptr = NULL;
		}
		delete _recv_pkt_ptr;
		_recv_pkt_ptr = NULL;
	}
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

		// Connect with timeout(block --> non-block --> block)
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
		
		u_long nblk = 1;
		ioctlsocket(_context.socket, FIONBIO, &nblk);
		::connect(_context.socket, (sockaddr *)&service, sizeof(sockaddr)); // Non-block, ignore return
		fd_set fds;
		timeval con_tv = { timeout_secs, 0 };
		FD_ZERO(&fds);
		FD_SET(_context.socket, &fds);
		if (select(NULL, NULL, &fds, NULL, &con_tv) <= 0) {
			status = RT_STATUS_SOCKET_ERR;
			break;
		}
		nblk = 0;
		ioctlsocket(_context.socket, FIONBIO, &nblk);

		// Set recv and send timeout
#ifdef WIN32
		int tv = timeout_secs * 1000;
#else
		timeval tv = { timeout_secs, 0 };
#endif
		if (0 != setsockopt(_context.socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv))) {
			status = RT_STATUS_SOCKET_ERR;
			break;
		}
		if (0 != setsockopt(_context.socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv))) {
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

		// "xxx" <----> "xxx"
		while (true)
		{
			status = _recv_packet(_recv_pkt_ptr);
			if (!rt_is_success(status))
				break;

			// Ignore these types here
			if (_recv_pkt_ptr->msg_type == RTMP_MSG_TYPE_AUDIO
				|| _recv_pkt_ptr->msg_type == RTMP_MSG_TYPE_VIDEO
				|| _recv_pkt_ptr->msg_type == RTMP_MSG_TYPE_INFO)
				continue;
			_handle_packet(_recv_pkt_ptr);

			if (_context.playing)
				break;
		}
	} while (false);

	return status;
}

void CRTMPClient::disconnect()
{
	if (-1 != _context.socket) {
		if (_context.stream_id > 0) {
			if (_context.mode == RTMP_MODE_PUSHER) {
				_invoke_fcunpublish();
			}
			_invoke_delete_stream();
			_context.stream_id = 0;
		}
		closesocket(_context.socket);
		_context.socket = -1;
	}
}

rt_status_t CRTMPClient::send_medadata(const rtmp_metadata_t &meta)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	if (-1 == _context.socket || !_context.playing) {
		status = RT_STATUS_UNINITIALIZED;
		return status;
	}

	rtmp_packet_t packet;
	packet.chk_type = RTMP_CHUNK_TYPE_LARGE;
	packet.chk_stream_id = RTMP_CHUNK_STREAM_OVER_CONNECTION2;
	packet.msg_type = RTMP_MSG_TYPE_INFO;
	packet.msg_stream_id = _context.stream_id; // Current stream id
	packet.timestamp = 0;

	uint8_t buffer[4096];
	packet.data_ptr = buffer + RTMP_MAX_HEADER_SIZE; // Reserved to fill header

	/////////////////////////////////////////////
	// Send media info
	uint8_t *ptr = packet.data_ptr;
	ptr = amf_encode_string(ptr, AVINIT("@setDataFrame"));
	ptr = amf_encode_string(ptr, AVINIT("onMetaData"));
	*ptr++ = AMF_OBJECT;
	ptr = amf_encode_named_string(ptr, AVINIT("copyright"), AVINIT("firehood"));
	ptr = amf_encode_named_number(ptr, AVINIT("width"), meta.width);
	ptr = amf_encode_named_number(ptr, AVINIT("height"), meta.height);
	ptr = amf_encode_named_number(ptr, AVINIT("framerate"), meta.fps);
	ptr = amf_encode_named_number(ptr, AVINIT("videodatarate"), meta.bitrate_kpbs);
	ptr = amf_encode_named_number(ptr, AVINIT("videocodecid"), 7); // 7:AVC(H264)
	if (meta.has_audio) {
		ptr = amf_encode_named_number(ptr, AVINIT("audiodatarate"), meta.datarate);
		ptr = amf_encode_named_number(ptr, AVINIT("audiosamplerate"), meta.samplerate);
		ptr = amf_encode_named_number(ptr, AVINIT("audiosamplesize"), meta.samples_per_frame);
		ptr = amf_encode_named_number(ptr, AVINIT("stereo"), meta.channels);
		ptr = amf_encode_named_number(ptr, AVINIT("audiocodecid"), 10); // 10:AAC
	}
	*ptr++ = 0x00;
	*ptr++ = 0x00;
	*ptr++ = AMF_OBJECT_END;
	packet.size = 4096;
	packet.valid = ptr - packet.data_ptr;
	status = _send_packet(&packet, false);
	if (!rt_is_success(status)) {
		return status;
	}

	/////////////////////////////////////////////
	// Send decode info
	// FLV video sequence format:
	// Frame type(4 bits) + codecID(4 bits) + AVCPacketType(1 bytes) + CompositionTime
	//	+ AVCDecoderConfiguration
	//
	ptr = packet.data_ptr;
	*ptr++ = 0x17;
	*ptr++ = 0x00;
	*ptr++ = 0x00;
	*ptr++ = 0x00;
	*ptr++ = 0x00;
	// AVCDecoderConfiguration  
	*ptr++ = 0x01;
	*ptr++ = meta.param.data_sps[1];
	*ptr++ = meta.param.data_sps[2];
	*ptr++ = meta.param.data_sps[3];
	*ptr++ = 0xff;
	// SPS
	*ptr++ = 0xE1;
	*ptr++ = meta.param.size_sps >> 8;
	*ptr++ = meta.param.size_sps & 0xff;
	memcpy(ptr, meta.param.data_sps, meta.param.size_sps);
	ptr += meta.param.size_sps;
	// PPS
	*ptr++ = 0x01;
	*ptr++ = meta.param.size_pps >> 8;
	*ptr++ = meta.param.size_pps & 0xff;
	memcpy(ptr, meta.param.data_pps, meta.param.size_pps);
	ptr += meta.param.size_pps;
	packet.msg_type = RTMP_MSG_TYPE_VIDEO;
	packet.valid = ptr - packet.data_ptr;
	status = _send_packet(&packet, false);
	if (!rt_is_success(status)) {
		return status;
	}

	return status;
}

rt_status_t CRTMPClient::send_video(uint32_t size, const uint8_t *data_ptr, uint64_t pts, bool keyframe)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	if (-1 == _context.socket || !_context.playing) {
		status = RT_STATUS_UNINITIALIZED;
		return status;
	}
	if (0 == size) {
		return status;
	}
	else if (NULL == data_ptr) {
		status = RT_STATUS_INVALID_PARAMETER;
		return status;
	}

	rtmp_packet_t packet;
	packet.chk_type = RTMP_CHUNK_TYPE_LARGE;
	packet.chk_stream_id = RTMP_CHUNK_STREAM_OVER_CONNECTION2;
	packet.msg_type = RTMP_MSG_TYPE_VIDEO;
	packet.msg_stream_id = _context.stream_id; // Current stream id
	packet.timestamp = pts;

	packet.size = size + RTMP_MAX_HEADER_SIZE * 2;
	uint8_t *reserved_ptr = new uint8_t[packet.size];
	if (NULL == reserved_ptr) {
		status = RT_STATUS_MEMORY_ALLOCATE;
		return status;
	}
	packet.data_ptr = reserved_ptr + RTMP_MAX_HEADER_SIZE;

	//
	// FLV video nalu format:
	// Frame type(4 bits) + codecID(4 bits) + AVCPacketType(1 bytes) + CompositionTime
	//	+ Nalu
	//
	uint8_t *ptr = packet.data_ptr;
	if (keyframe) {
		*ptr++ = 0x17;
	}
	else {
		*ptr++ = 0x27;
	}
	*ptr++ = 0x01;
	*ptr++ = 0x00;
	*ptr++ = 0x00;
	*ptr++ = 0x00;
	*ptr++ = size >> 24;
	*ptr++ = size >> 16;
	*ptr++ = size >> 8;
	*ptr++ = size & 0xff;
	memcpy(ptr, data_ptr, size);
	ptr += size;
	packet.valid = ptr - packet.data_ptr;
	status = _send_packet(&packet, false);

	delete[] reserved_ptr;

	return status;
}

rt_status_t CRTMPClient::send_audio(uint32_t size, const uint8_t *data_ptr, uint64_t pts)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	if (-1 == _context.socket || !_context.playing) {
		status = RT_STATUS_UNINITIALIZED;
		return status;
	}
	if (0 == size) {
		return status;
	}
	else if (NULL == data_ptr) {
		status = RT_STATUS_INVALID_PARAMETER;
		return status;
	}

	rtmp_packet_t packet;
	packet.chk_type = RTMP_CHUNK_TYPE_LARGE;
	packet.chk_stream_id = RTMP_CHUNK_STREAM_OVER_CONNECTION2;
	packet.msg_type = RTMP_MSG_TYPE_AUDIO;
	packet.msg_stream_id = _context.stream_id; // Current stream id
	packet.timestamp = pts;

	packet.size = size + RTMP_MAX_HEADER_SIZE * 2;
	uint8_t *reserved_ptr = new uint8_t[packet.size];
	if (NULL == reserved_ptr) {
		status = RT_STATUS_MEMORY_ALLOCATE;
		return status;
	}
	packet.data_ptr = reserved_ptr + RTMP_MAX_HEADER_SIZE;

	uint8_t *ptr = packet.data_ptr;
	*ptr++ = 0xAF;
	*ptr++ = 0x01;
	memcpy(ptr, data_ptr, size);
	ptr += size;
	packet.valid = ptr - packet.data_ptr;
	status = _send_packet(&packet, false);

	delete[] reserved_ptr;

	return status;
}

rt_status_t CRTMPClient::recv_packet(rtmp_packet_t *pkt_ptr)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	status = _recv_packet(pkt_ptr);

	return status;
}

rt_status_t CRTMPClient::handle_packet(rtmp_packet_t *pkt_ptr)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	status = _handle_packet(pkt_ptr);

	return status;
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

		// Params: tcUrl
		if (_context.params.tcUrl.empty()) {
			_context.params.tcUrl = url_str.substr(0, url_str.length() - _context.link.stream.length() - 1);
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
		if (!_recv(RTMP_HANDSHAKE_SIG_SIZE, S1)) {
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
	packet.chk_type = RTMP_CHUNK_TYPE_LARGE;
	packet.chk_stream_id = RTMP_CHUNK_STREAM_OVER_CONNECTION;
	packet.msg_type = RTMP_MSG_TYPE_INVOKE;
	packet.msg_stream_id = 0; // Because of no stream now
	packet.timestamp = 0;

	//
	// "connect":
	// +-------------------------+---------+--------------------------------------------------------------+
	// |         Field Name      |   Type  |          Description                                         |
	// +-------------------------+---------+--------------------------------------------------------------+
	// |      Command Name       |  String | Name of the command.Set to "connect".                        |
	// +-------------------------+---------+--------------------------------------------------------------+
	// |       Transaction ID    |  Number | Always set to 1.                                             |
	// +-------------------------+---------+--------------------------------------------------------------+
	// |       Command Object    |  Object | Command information object which has the name - value pairs. |
	// +-------------------------+---------+--------------------------------------------------------------+
	// | Optional User Arguments |  Object | Any optional information                                     |
	// +-------------------------+---------+--------------------------------------------------------------+
	//
	// pairs:...
	//
	uint8_t buffer[4096];
	packet.data_ptr = buffer + RTMP_MAX_HEADER_SIZE; // Reserved to fill header
	uint8_t *ptr = packet.data_ptr;
	ptr = amf_encode_string(ptr, av_connect);
	ptr = amf_encode_number(ptr, ++_context.invoke_ids);
	*ptr++ = AMF_OBJECT;
	val_t app = { _context.link.app.length(), _context.link.app };
	ptr = amf_encode_named_string(ptr, av_app, app);
	if (_context.mode == RTMP_MODE_PUSHER) {
		ptr = amf_encode_named_string(ptr, av_type, av_nonprivate);
	}
	if (!_context.params.flashVer.empty()) {
		val_t flashVer = { _context.params.flashVer.length(), _context.params.flashVer };
		ptr = amf_encode_named_string(ptr, av_flashVer, flashVer);
	}
	if (!_context.params.swfUrl.empty()) {
		val_t swfUrl = { _context.params.swfUrl.length(), _context.params.swfUrl };
		ptr = amf_encode_named_string(ptr, av_swfUrl, swfUrl);
	}
	if (!_context.params.tcUrl.empty()) {
		val_t tcUrl = { _context.params.tcUrl.length(), _context.params.tcUrl };
		ptr = amf_encode_named_string(ptr, av_tcUrl, tcUrl);
	}
	if (_context.mode != RTMP_MODE_PUSHER) {
		ptr = amf_encode_named_boolean(ptr, av_fpad, false);
		ptr = amf_encode_named_number(ptr, av_capabilities, 15.0);
		ptr = amf_encode_named_number(ptr, av_audioCodecs, 3191.0);
		ptr = amf_encode_named_number(ptr, av_videoCodecs, 252.0);
		ptr = amf_encode_named_number(ptr, av_videoFunction, 1.0);
		if (!_context.params.pageUrl.empty()) {
			val_t pageUrl = { _context.params.pageUrl.length(), _context.params.pageUrl };
			ptr = amf_encode_named_string(ptr, av_pageUrl, pageUrl);
		}
	}
	if (0 != _context.params.encoding) {
		ptr = amf_encode_named_number(ptr, av_objectEncoding, _context.params.encoding);
	}
	*ptr++ = 0x00;
	*ptr++ = 0x00;
	*ptr++ = AMF_OBJECT_END; // end of object - 0x00 0x00 0x09
	if (!_context.params.auth.empty()) {
		val_t auth = { _context.params.auth.length(), _context.params.auth };
		ptr = amf_encode_boolean(ptr, true);
		ptr = amf_encode_string(ptr, auth);
	}
	//if (_context.params.extras.num > 0) {
	//	for (int i = 0; i < _context.params.extras.num; i++) {
	//		ptr = amf_encode_prop(ptr, _context.params.extras.props[i]);
	//	}
	//}
	packet.size = 4096;
	packet.valid = ptr - packet.data_ptr;

	return _send_packet(&packet, true);
}

rt_status_t CRTMPClient::_invoke_release_stream()
{
	rt_status_t status = RT_STATUS_SUCCESS;

	rtmp_packet_t packet;
	packet.chk_type = RTMP_CHUNK_TYPE_LARGE;
	packet.chk_stream_id = RTMP_CHUNK_STREAM_OVER_CONNECTION;
	packet.msg_type = RTMP_MSG_TYPE_INVOKE;
	packet.msg_stream_id = 0; // Because of no stream now
	packet.timestamp = 0;

	uint8_t buffer[4096];
	packet.data_ptr = buffer + RTMP_MAX_HEADER_SIZE; // Reserved to fill header
	uint8_t *ptr = packet.data_ptr;
	ptr = amf_encode_string(ptr, av_releaseStream);
	ptr = amf_encode_number(ptr, ++_context.invoke_ids);
	*ptr++ = AMF_NULL;
	val_t playpath = { _context.link.stream.length(), _context.link.stream };
	ptr = amf_encode_string(ptr, playpath);
	packet.size = 4096;
	packet.valid = ptr - packet.data_ptr;

	return _send_packet(&packet, false);
}

rt_status_t CRTMPClient::_invoke_fcpublish()
{
	rt_status_t status = RT_STATUS_SUCCESS;

	rtmp_packet_t packet;
	packet.chk_type = RTMP_CHUNK_TYPE_LARGE;
	packet.chk_stream_id = RTMP_CHUNK_STREAM_OVER_CONNECTION;
	packet.msg_type = RTMP_MSG_TYPE_INVOKE;
	packet.msg_stream_id = 0; // Because of no stream now
	packet.timestamp = 0;

	uint8_t buffer[4096];
	packet.data_ptr = buffer + RTMP_MAX_HEADER_SIZE; // Reserved to fill header
	uint8_t *ptr = packet.data_ptr;
	ptr = amf_encode_string(ptr, av_FCPublish);
	ptr = amf_encode_number(ptr, ++_context.invoke_ids);
	*ptr++ = AMF_NULL;
	val_t playpath = { _context.link.stream.length(), _context.link.stream };
	ptr = amf_encode_string(ptr, playpath);
	packet.size = 4096;
	packet.valid = ptr - packet.data_ptr;

	return _send_packet(&packet, false);
}

rt_status_t CRTMPClient::_invoke_create_stream()
{
	rt_status_t status = RT_STATUS_SUCCESS;

	rtmp_packet_t packet;
	packet.chk_type = RTMP_CHUNK_TYPE_LARGE;
	packet.chk_stream_id = RTMP_CHUNK_STREAM_OVER_CONNECTION;
	packet.msg_type = RTMP_MSG_TYPE_INVOKE;
	packet.msg_stream_id = 0; // Because of no stream now
	packet.timestamp = 0;

	uint8_t buffer[4096];
	packet.data_ptr = buffer + RTMP_MAX_HEADER_SIZE; // Reserved to fill header
	uint8_t *ptr = packet.data_ptr;
	ptr = amf_encode_string(ptr, av_createStream);
	ptr = amf_encode_number(ptr, ++_context.invoke_ids);
	*ptr++ = AMF_NULL;
	packet.size = 4096;
	packet.valid = ptr - packet.data_ptr;

	return _send_packet(&packet, true);
}

rt_status_t CRTMPClient::_invoke_publish()
{
	rt_status_t status = RT_STATUS_SUCCESS;

	rtmp_packet_t packet;
	packet.chk_type = RTMP_CHUNK_TYPE_LARGE;
	packet.chk_stream_id = RTMP_CHUNK_STREAM_OVER_CONNECTION2;
	packet.msg_type = RTMP_MSG_TYPE_INVOKE;
	packet.msg_stream_id = _context.stream_id; // Because of has stream now
	packet.timestamp = 0;

	uint8_t buffer[4096];
	packet.data_ptr = buffer + RTMP_MAX_HEADER_SIZE; // Reserved to fill header
	uint8_t *ptr = packet.data_ptr;
	ptr = amf_encode_string(ptr, av_publish);
	ptr = amf_encode_number(ptr, ++_context.invoke_ids);
	*ptr++ = AMF_NULL;
	val_t playpath = { _context.link.stream.length(), _context.link.stream };
	ptr = amf_encode_string(ptr, playpath);
	ptr = amf_encode_string(ptr, av_live);
	packet.size = 4096;
	packet.valid = ptr - packet.data_ptr;

	return _send_packet(&packet, true);
}

rt_status_t CRTMPClient::_invoke_fcunpublish()
{
	rt_status_t status = RT_STATUS_SUCCESS;

	rtmp_packet_t packet;
	packet.chk_type = RTMP_CHUNK_TYPE_LARGE;
	packet.chk_stream_id = RTMP_CHUNK_STREAM_OVER_CONNECTION;
	packet.msg_type = RTMP_MSG_TYPE_INVOKE;
	packet.msg_stream_id = _context.stream_id; // 0 or current stream id?
	packet.timestamp = 0;

	uint8_t buffer[4096];
	packet.data_ptr = buffer + RTMP_MAX_HEADER_SIZE; // Reserved to fill header
	uint8_t *ptr = packet.data_ptr;
	ptr = amf_encode_string(ptr, av_FCUnpublish);
	ptr = amf_encode_number(ptr, ++_context.invoke_ids);
	*ptr++ = AMF_NULL;
	val_t playpath = { _context.link.stream.length(), _context.link.stream };
	ptr = amf_encode_string(ptr, playpath);
	packet.size = 4096;
	packet.valid = ptr - packet.data_ptr;

	return _send_packet(&packet, false);
}

rt_status_t CRTMPClient::_invoke_delete_stream()
{
	rt_status_t status = RT_STATUS_SUCCESS;

	rtmp_packet_t packet;
	packet.chk_type = RTMP_CHUNK_TYPE_LARGE;
	packet.chk_stream_id = RTMP_CHUNK_STREAM_OVER_CONNECTION;
	packet.msg_type = RTMP_MSG_TYPE_INVOKE;
	packet.msg_stream_id = _context.stream_id; // 0 or current stream id?
	packet.timestamp = 0;

	uint8_t buffer[4096];
	packet.data_ptr = buffer + RTMP_MAX_HEADER_SIZE; // Reserved to fill header
	uint8_t *ptr = packet.data_ptr;
	ptr = amf_encode_string(ptr, av_deleteStream);
	ptr = amf_encode_number(ptr, ++_context.invoke_ids);
	*ptr++ = AMF_NULL;
	ptr = amf_encode_number(ptr, _context.stream_id);
	packet.size = 4096;
	packet.valid = ptr - packet.data_ptr;

	return _send_packet(&packet, false);
}

rt_status_t CRTMPClient::_invoke_server_bw()
{
	rt_status_t status = RT_STATUS_SUCCESS;

	rtmp_packet_t packet;
	packet.chk_type = RTMP_CHUNK_TYPE_LARGE;
	packet.chk_stream_id = RTMP_CHUNK_STREAM_PROTOCOL_CONTROL;
	packet.msg_type = RTMP_MSG_TYPE_SERVER_BW;
	packet.msg_stream_id = _context.stream_id; // 0 or current stream id?
	packet.timestamp = 0;

	uint8_t buffer[4096];
	packet.data_ptr = buffer + RTMP_MAX_HEADER_SIZE; // Reserved to fill header
	uint8_t *ptr = packet.data_ptr;
	amf_encode_u32(ptr, _context.server_bw);
	ptr += 4;
	packet.size = 4096;
	packet.valid = ptr - packet.data_ptr;

	return _send_packet(&packet, false);
}

/*
from http://jira.red5.org/confluence/display/docs/Ping:

Ping is the most mysterious message in RTMP and till now we haven't fully interpreted it yet. In summary, Ping message is used as a special command that are exchanged between client and server. This page aims to document all known Ping messages. Expect the list to grow.

The type of Ping packet is 0x4 and contains two mandatory parameters and two optional parameters. The first parameter is the type of Ping and in short integer. The second parameter is the target of the ping. As Ping is always sent in Channel 2 (control channel) and the target object in RTMP header is always 0 which means the Connection object, it's necessary to put an extra parameter to indicate the exact target object the Ping is sent to. The second parameter takes this responsibility. The value has the same meaning as the target object field in RTMP header. (The second value could also be used as other purposes, like RTT Ping/Pong. It is used as the timestamp.) The third and fourth parameters are optional and could be looked upon as the parameter of the Ping packet. Below is an unexhausted list of Ping messages.

* type 0: Clear the stream. No third and fourth parameters. The second parameter could be 0. After the connection is established, a Ping 0,0 will be sent from server to client. The message will also be sent to client on the start of Play and in response of a Seek or Pause/Resume request. This Ping tells client to re-calibrate the clock with the timestamp of the next packet server sends.
* type 1: Tell the stream to clear the playing buffer.
* type 3: Buffer time of the client. The third parameter is the buffer time in millisecond.
* type 4: Reset a stream. Used together with type 0 in the case of VOD. Often sent before type 0.
* type 6: Ping the client from server. The second parameter is the current time.
* type 7: Pong reply from client. The second parameter is the time the server sent with his ping request.
* type 26: SWFVerification request
* type 27: SWFVerification response
*/
rt_status_t CRTMPClient::_invoke_ctrl(uint16_t type, uint32_t object, uint32_t time_ms)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	rtmp_packet_t packet;
	packet.chk_type = RTMP_CHUNK_TYPE_LARGE;
	packet.chk_stream_id = RTMP_CHUNK_STREAM_PROTOCOL_CONTROL;
	packet.msg_type = RTMP_MSG_TYPE_CONTROL;
	packet.msg_stream_id = _context.stream_id; // 0 or current stream id?
	packet.timestamp = 0;

	uint32_t size = 0;
	switch (type)
	{
	case 0x03: // buffer time
		size = 10;
		break;
	case 0x1A: // SWF verify request
		size = 3;
		break;
	case 0x1B: // SWF verify response
		size = 44;
		break;
	default:
		size = 6;
		break;
	}

	uint8_t buffer[4096];
	packet.data_ptr = buffer + RTMP_MAX_HEADER_SIZE; // Reserved to fill header
	uint8_t *ptr = packet.data_ptr;
	ptr = amf_encode_u16(ptr, type);
	if (type == 0x1B) {
		// 42 bytes
		// ...
	}
	else if (type == 0x1A) {
		*ptr++ = object & 0xff;
	}
	else {
		if (size > 2) {
			ptr = amf_encode_u32(ptr, object);
		}
		if (size > 6) {
			ptr = amf_encode_u32(ptr, time_ms);
		}
	}
	packet.size = 4096;
	packet.valid = size;

	return _send_packet(&packet, false);
}

rt_status_t CRTMPClient::_invoke_fcsubscribe()
{
	rt_status_t status = RT_STATUS_SUCCESS;

	rtmp_packet_t packet;
	packet.chk_type = RTMP_CHUNK_TYPE_LARGE;
	packet.chk_stream_id = RTMP_CHUNK_STREAM_OVER_CONNECTION;
	packet.msg_type = RTMP_MSG_TYPE_INVOKE;
	packet.msg_stream_id = _context.stream_id; // 0 or current stream id?
	packet.timestamp = 0;

	uint8_t buffer[4096];
	packet.data_ptr = buffer + RTMP_MAX_HEADER_SIZE; // Reserved to fill header
	uint8_t *ptr = packet.data_ptr;
	ptr = amf_encode_string(ptr, av_FCSubscribe);
	ptr = amf_encode_number(ptr, ++_context.invoke_ids);
	*ptr++ = AMF_NULL;
	val_t stream = { _context.link.stream.length(), _context.link.stream };
	ptr = amf_encode_string(ptr, stream);
	packet.size = 4096;
	packet.valid = ptr - packet.data_ptr;

	return _send_packet(&packet, true);
}

rt_status_t CRTMPClient::_invoke_play()
{
	rt_status_t status = RT_STATUS_SUCCESS;

	rtmp_packet_t packet;
	packet.chk_type = RTMP_CHUNK_TYPE_LARGE;
	packet.chk_stream_id = RTMP_CHUNK_STREAM_OVER_STREAM2;
	packet.msg_type = RTMP_MSG_TYPE_INVOKE;
	packet.msg_stream_id = _context.stream_id; // Current stream id
	packet.timestamp = 0;

	uint8_t buffer[4096];
	packet.data_ptr = buffer + RTMP_MAX_HEADER_SIZE; // Reserved to fill header
	uint8_t *ptr = packet.data_ptr;
	ptr = amf_encode_string(ptr, av_play);
	ptr = amf_encode_number(ptr, ++_context.invoke_ids);
	*ptr++ = AMF_NULL;
	val_t stream = { _context.link.stream.length(), _context.link.stream };
	ptr = amf_encode_string(ptr, stream);
	ptr = amf_encode_number(ptr, -1000.0);
	packet.size = 4096;
	packet.valid = ptr - packet.data_ptr;

	return _send_packet(&packet, true);
}

rt_status_t CRTMPClient::_handle_chunk_size(rtmp_packet_t *pkt_ptr)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	if (pkt_ptr->valid >= 4) {
		_context.in_chunk_size = amf_decode_u32(pkt_ptr->data_ptr);
		LogD(TAG_RTMP, "In chunk size = %d", _context.in_chunk_size);
	}

	return status;
}

rt_status_t CRTMPClient::_handle_bytes_read_report(rtmp_packet_t *pkt_ptr)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	// Nothing to do...

	return status;
}

rt_status_t CRTMPClient::_handle_control(rtmp_packet_t *pkt_ptr)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	uint16_t ctrl_type = 0;
	if (pkt_ptr->valid >= 2) {
		ctrl_type = amf_decode_u16(pkt_ptr->data_ptr);
	}

	uint32_t tmp = 0;
	if (pkt_ptr->valid >= 6) {
		switch (ctrl_type)
		{
		case 0x00: // Stream Begin
			tmp = amf_decode_u32(pkt_ptr->data_ptr + 2);
			break;
		case 0x01: // Stream EOF
			tmp = amf_decode_u32(pkt_ptr->data_ptr + 2);
			//if (r->m_pausing == 1)
			//	r->m_pausing = 2;
			break;
		case 0x02: // Stream Dry
			tmp = amf_decode_u32(pkt_ptr->data_ptr + 2);
			break;
		case 0x04: // Stream IsRecorded
			tmp = amf_decode_u32(pkt_ptr->data_ptr + 2);
			break;
		case 0x06: // Server Ping. reply with pong.
			tmp = amf_decode_u32(pkt_ptr->data_ptr + 2);
			_invoke_ctrl(0x07, tmp, 0);
			break;
			/* FMS 3.5 servers send the following two controls to let the client
			* know when the server has sent a complete buffer. I.e., when the
			* server has sent an amount of data equal to m_nBufferMS in duration.
			* The server meters its output so that data arrives at the client
			* in realtime and no faster.
			*
			* The rtmpdump program tries to set m_nBufferMS as large as
			* possible, to force the server to send data as fast as possible.
			* In practice, the server appears to cap this at about 1 hour's
			* worth of data. After the server has sent a complete buffer, and
			* sends this BufferEmpty message, it will wait until the play
			* duration of that buffer has passed before sending a new buffer.
			* The BufferReady message will be sent when the new buffer starts.
			* (There is no BufferReady message for the very first buffer;
			* presumably the Stream Begin message is sufficient for that
			* purpose.)
			*
			* If the network speed is much faster than the data bitrate, then
			* there may be long delays between the end of one buffer and the
			* start of the next.
			*
			* Since usually the network allows data to be sent at
			* faster than realtime, and rtmpdump wants to download the data
			* as fast as possible, we use this RTMP_LF_BUFX hack: when we
			* get the BufferEmpty message, we send a Pause followed by an
			* Unpause. This causes the server to send the next buffer immediately
			* instead of waiting for the full duration to elapse. (That's
			* also the purpose of the ToggleStream function, which rtmpdump
			* calls if we get a read timeout.)
			*
			* Media player apps don't need this hack since they are just
			* going to play the data in realtime anyway. It also doesn't work
			* for live streams since they obviously can only be sent in
			* realtime. And it's all moot if the network speed is actually
			* slower than the media bitrate.
			*/
		case 0x1F: // Stream BufferEmpty
			tmp = amf_decode_u32(pkt_ptr->data_ptr + 2);
			//if (!(r->Link.lFlags & RTMP_LF_BUFX))
			//	break;
			//if (!r->m_pausing) {
			//	r->m_pauseStamp = r->m_mediaChannel < r->m_channelsAllocatedIn ?
			//		r->m_channelTimestamp[r->m_mediaChannel] : 0;
			//	RTMP_SendPause(r, TRUE, r->m_pauseStamp);
			//	r->m_pausing = 1;
			//}
			//else if (r->m_pausing == 2) {
			//	RTMP_SendPause(r, FALSE, r->m_pauseStamp);
			//	r->m_pausing = 3;
			//}
			break;
		case 0x20: // Stream BufferReady
			tmp = amf_decode_u32(pkt_ptr->data_ptr + 2);
			break;
		default: // Stream xx
			tmp = amf_decode_u32(pkt_ptr->data_ptr + 2);
			break;
		}
	}

	return status;
}

rt_status_t CRTMPClient::_handle_server_bw(rtmp_packet_t *pkt_ptr)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	if (pkt_ptr->valid >= 4) {
		_context.server_bw = amf_decode_u32(pkt_ptr->data_ptr);
		LogD(TAG_RTMP, "Server bandwidth = %d", _context.server_bw);
	}

	return status;
}

rt_status_t CRTMPClient::_handle_client_bw(rtmp_packet_t *pkt_ptr)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	if (pkt_ptr->valid >= 4) {
		_context.client_bw = amf_decode_u32(pkt_ptr->data_ptr);
		LogD(TAG_RTMP, "Client bandwidth = %d", _context.client_bw);
	}
	_context.client_bw2 = 0;
	if (pkt_ptr->valid > 4) {
		_context.client_bw2 = pkt_ptr->data_ptr[4];
		LogD(TAG_RTMP, "Client bandwidth2 = %d", _context.client_bw2);
	}

	return status;
}

rt_status_t CRTMPClient::_handle_audio(rtmp_packet_t *pkt_ptr)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	//if (!r->m_mediaChannel)
	//	r->m_mediaChannel = packet->m_nChannel;
	//if (!r->m_pausing)
	//	r->m_mediaStamp = packet->m_nTimeStamp;

	return status;
}

rt_status_t CRTMPClient::_handle_video(rtmp_packet_t *pkt_ptr)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	//if (!r->m_mediaChannel)
	//	r->m_mediaChannel = packet->m_nChannel;
	//if (!r->m_pausing)
	//	r->m_mediaStamp = packet->m_nTimeStamp;

	return status;
}

rt_status_t CRTMPClient::_handle_flex_stream_send(rtmp_packet_t *pkt_ptr)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	// Nothing to do...

	return status;
}

rt_status_t CRTMPClient::_handle_flex_shared_object(rtmp_packet_t *pkt_ptr)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	// Nothing to do...

	return status;
}

rt_status_t CRTMPClient::_handle_flex_message(rtmp_packet_t *pkt_ptr)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	// FIXME:
	//_handle_invoke();

	return status;
}

rt_status_t CRTMPClient::_handle_info(rtmp_packet_t *pkt_ptr)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	/*
	// allright we get some info here, so parse it and print it
	// also keep duration or filesize to make a nice progress bar
	AMFObject obj;
	AVal metastring;
	int ret = FALSE;

	int nRes = AMF_Decode(&obj, body, len, FALSE);
	if (nRes < 0)
	{
		RTMP_Log(RTMP_LOGERROR, "%s, error decoding meta data packet", __FUNCTION__);
		return FALSE;
	}

	AMF_Dump(&obj);
	AMFProp_GetString(AMF_GetProp(&obj, NULL, 0), &metastring);

	if (AVMATCH(&metastring, &av_onMetaData))
	{
		AMFObjectProperty prop;
		// Show metadata
		RTMP_Log(RTMP_LOGINFO, "Metadata:");
		DumpMetaData(&obj);
		if (RTMP_FindFirstMatchingProperty(&obj, &av_duration, &prop))
		{
			r->m_fDuration = prop.p_vu.p_number;
			// RTMP_Log(RTMP_LOGDEBUG, "Set duration: %.2f", m_fDuration);
		}
		// Search for audio or video tags
		if (RTMP_FindPrefixProperty(&obj, &av_video, &prop))
			r->m_read.dataType |= 1;
		if (RTMP_FindPrefixProperty(&obj, &av_audio, &prop))
			r->m_read.dataType |= 4;
		ret = TRUE;
	}
	AMF_Reset(&obj);
	*/

	return status;
}

rt_status_t CRTMPClient::_handle_shared_object(rtmp_packet_t *pkt_ptr)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	// Nothing to do...

	return status;
}

rt_status_t CRTMPClient::_handle_invoke(rtmp_packet_t *pkt_ptr)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	//
	// Object begin
	//     Property: <name, string>		// Method
	//     Property: <name, number>		// id
	// ......
	// Object end
	//
	uint32_t data_size = pkt_ptr->valid;
	uint8_t *ptr = pkt_ptr->data_ptr;
	amf_object_t obj;
	do {
		// Parse the received data to AMF/AMF3 object...
		if (ptr[0] != AMF_STRING) {
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}
		int ret = amf_decode(ptr, obj, data_size, false);
		if (ret < 0) {
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}
		amf_dump(obj);

		amf_object_property_t prop = obj.props[0];
		if (AMF_STRING != prop.type) {
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}
		val_t method = prop.value;
		prop = obj.props[1];
		if (prop.type != AMF_NUMBER) {
			status = RT_STATUS_INVALID_PARAMETER;
			break;
		}
		uint64_t id = prop.number;
		LogD(TAG_RTMP, "invoke type = %s, invoke id = %d", method.value.c_str(), id);

		// Method - _result
		if (AVMATCH(&method, &av__result)) {
			val_t method_invoked = { 0, "" };

			std::vector<rtmp_invoke_t>::iterator iter;
			for (iter = _context.invokes.begin(); iter != _context.invokes.end(); iter++) {
				rtmp_invoke_t invoke = (*iter);
				if (invoke.id == id) {
					method_invoked.value = invoke.invoke;
					method_invoked.len = invoke.invoke.length();
					_context.invokes.erase(iter);
					break;
				}
			}
			if (0 == method_invoked.len) {
				status = RT_STATUS_INVALID_PARAMETER;
				break;
			}

			// Method - _result - "connect"
			if (AVMATCH(&method_invoked, &av_connect)) {
				// Object begin
				//     Property: <name, string>			_result
				//     Property: <name, number>			0.00
				//     Object begin
				//         Property: <name, string>		FMS/3,0,1,123
				//         Property: <name, number>		31.00
				//     Object end
				//     Object begin
				//         Property: <name, string>		status
				//         Property: <name, string>		NetConnection.Connect.Success
				//         Property: <name, string>		Connection succeeded.
				//         Property: <name, number>		0.00
				//     Object end
				// Object end

				if (_context.mode == RTMP_MODE_PUSHER) {
					_invoke_release_stream();
					_invoke_fcpublish();
				}
				else {
					_invoke_server_bw();
					_invoke_ctrl(3, 0, 300);
				}

				_invoke_create_stream();

				if (_context.mode != RTMP_MODE_PUSHER) {
					_invoke_fcsubscribe();
				}
			}
			// Method - _result - "createStream"
			else if (AVMATCH(&method_invoked, &av_createStream)) {
				// Object begin
				//     Property: <name, string>			_result
				//     Property: <name, number>			4.00
				//     Property: null
				//     Property: <name, number>			1.00
				// Object end

				prop = obj.props[3];
				if (AMF_NUMBER != prop.type) {
					status = RT_STATUS_INVALID_PARAMETER;
					break;
				}
				_context.stream_id = (uint32_t)prop.number;

				if (_context.mode == RTMP_MODE_PUSHER) {
					_invoke_publish();
				}
				else {
					_invoke_play();
					_invoke_ctrl(3, _context.stream_id, _context.buffer_ms);
				}
			}
			else if (AVMATCH(&method_invoked, &av_play) || AVMATCH(&method_invoked, &av_publish)) {
				_context.playing = true;
			}
		}
		// Method: onBWDone
		else if (AVMATCH(&method, &av_onBWDone)) {
			//if (!r->m_nBWCheckCounter)
			//	SendCheckBW(r);
		}
		// Method: onFCSubscribe
		else if (AVMATCH(&method, &av_onFCSubscribe)) {
			// SendOnFCSubscribe();
		}
		// Method: onFCUnsubscribe
		else if (AVMATCH(&method, &av_onFCUnsubscribe)) {
		}
		// Method: ping
		else if (AVMATCH(&method, &av_ping)) {
			//SendPong(r, txn);
		}
		// Method: _onbwcheck
		else if (AVMATCH(&method, &av__onbwcheck)) {
			//SendCheckBWResult(r, txn);
		}
		// Method: _onbwdone
		else if (AVMATCH(&method, &av__onbwdone)) {
			std::vector<rtmp_invoke_t>::iterator iter;
			for (iter = _context.invokes.begin(); iter != _context.invokes.end(); iter++) {
				rtmp_invoke_t invoke = (*iter);
				if (0 == invoke.invoke.compare(av__checkbw.value)) {
					_context.invokes.erase(iter);
					break;
				}
			}
		}
		// Method: _error
		else if (AVMATCH(&method, &av__error)) {
		}
		// Method: close
		else if (AVMATCH(&method, &av_close)) {
			//RTMP_Close(r);
		}
		// Method - onStatus
		else if (AVMATCH(&method, &av_onStatus)) {
			// Object begin
			//     Property: <name, string>			onStatus
			//     Property: <name, number>			0.00
			//     null
			//     Object begin
			//         Property: <name, string>		level, status
			//         Property: <name, string>		code, NetStream.Publish.Start
			//         Property: <name, string>		description, Start publishing
			//     Object end
			// Object end

			prop = obj.props[3];
			if (AMF_OBJECT != prop.type) {
				status = RT_STATUS_INVALID_PARAMETER;
				break;
			}
			amf_object_t obj2 = prop.object;
			val_t code, level;
			for (uint32_t i = 0; i < obj2.num; i++) {
				if (0 == obj2.props[i].name.value.compare(av_code.value)) {
					code = obj2.props[i].value;
				}
				else if (0 == obj2.props[i].name.value.compare(av_level.value)) {
					level = obj2.props[i].value;
				}
			}

			// Method - onStatus - "NetStream.Failed"/"NetStream.Play.Failed"/"NetStream.Play.StreamNotFound"/
			// "NetConnection.Connect.InvalidApp"
			if (AVMATCH(&code, &av_NetStream_Failed)
				|| AVMATCH(&code, &av_NetStream_Play_Failed)
				|| AVMATCH(&code, &av_NetStream_Play_StreamNotFound)
				|| AVMATCH(&code, &av_NetConnection_Connect_InvalidApp)) {
				status = RT_STATUS_REQUIRED_CLOSE;
				disconnect();
			}
			// Method - onStatus - "NetStream.Play.Start"/"NetStream.Play.PublishNotify"
			else if (AVMATCH(&code, &av_NetStream_Play_Start)
				|| AVMATCH(&code, &av_NetStream_Play_PublishNotify)) {
				_context.playing = true;
				std::vector<rtmp_invoke_t>::iterator iter;
				for (iter = _context.invokes.begin(); iter != _context.invokes.end(); iter++) {
					rtmp_invoke_t invoke = (*iter);
					if (0 == invoke.invoke.compare(av_play.value)) {
						_context.invokes.erase(iter);
						break;
					}
				}
			}
			// Method - onStatus - "NetStream.Publish.Start"
			else if (AVMATCH(&code, &av_NetStream_Publish_Start)) {
				_context.playing = true;
				std::vector<rtmp_invoke_t>::iterator iter;
				for (iter = _context.invokes.begin(); iter != _context.invokes.end(); iter++) {
					rtmp_invoke_t invoke = (*iter);
					if (0 == invoke.invoke.compare(av_publish.value)) {
						_context.invokes.erase(iter);
						break;
					}
				}
			}
			// Method - onStatus - "NetStream.Play.Complete"/"NetStream.Play.Stop"/"NetStream.Play.UnpublishNotify"
			else if (AVMATCH(&code, &av_NetStream_Play_Complete)
				|| AVMATCH(&code, &av_NetStream_Play_Stop)
				|| AVMATCH(&code, &av_NetStream_Play_UnpublishNotify)) {
				status = RT_STATUS_REQUIRED_CLOSE;
				disconnect();
			}
			// Method - onStatus - "NetStream.Seek.Notify"
			else if (AVMATCH(&code, &av_NetStream_Seek_Notify)) {
				//r->m_read.flags &= ~RTMP_READ_SEEKING;
			}
			// Method - onStatus - "NetStream.Pause.Notify"
			else if (AVMATCH(&code, &av_NetStream_Pause_Notify)) {
				//if (r->m_pausing == 1 || r->m_pausing == 2)
				//{
				//	RTMP_SendPause(r, FALSE, r->m_pauseStamp);
				//	r->m_pausing = 3;
				//}
			}
		}
		// Method: playlist_ready
		else if (AVMATCH(&method, &av_playlist_ready)) {
			std::vector<rtmp_invoke_t>::iterator iter;
			for (iter = _context.invokes.begin(); iter != _context.invokes.end(); iter++) {
				rtmp_invoke_t invoke = (*iter);
				if (0 == invoke.invoke.compare(av_set_playlist.value)) {
					_context.invokes.erase(iter);
					break;
				}
			}
		}
		else{
			// ...
		}
	} while (false);

	return status;
}

rt_status_t CRTMPClient::_handle_flash_video(rtmp_packet_t *pkt_ptr)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	return status;
}

rt_status_t CRTMPClient::_handle_packet(rtmp_packet_t *pkt_ptr)
{
	rt_status_t status = RT_STATUS_SUCCESS;

	switch (pkt_ptr->msg_type)
	{
	//case RTMP_MSG_TYPE_RESERVED00:
	//	break;
	case RTMP_MSG_TYPE_CHUNK_SIZE:
		LogD(TAG_RTMP, "%s - %s", __FUNCTION__, "RTMP_MSG_TYPE_CHUNK_SIZE");
		status = _handle_chunk_size(pkt_ptr);
		break;
	//case RTMP_MSG_TYPE_RESERVED02:
	//	break;
	case RTMP_MSG_TYPE_BYTES_READ_REPORT:
		LogD(TAG_RTMP, "%s - %s", __FUNCTION__, "RTMP_MSG_TYPE_BYTES_READ_REPORT");
		status = _handle_bytes_read_report(pkt_ptr);
		break;
	case RTMP_MSG_TYPE_CONTROL:
		LogD(TAG_RTMP, "%s - %s", __FUNCTION__, "RTMP_MSG_TYPE_CONTROL");
		status = _handle_control(pkt_ptr);
		break;
	case RTMP_MSG_TYPE_SERVER_BW:
		LogD(TAG_RTMP, "%s - %s", __FUNCTION__, "RTMP_MSG_TYPE_SERVER_BW");
		status = _handle_server_bw(pkt_ptr);
		break;
	case RTMP_MSG_TYPE_CLIENT_BW:
		LogD(TAG_RTMP, "%s - %s", __FUNCTION__, "RTMP_MSG_TYPE_CLIENT_BW");
		status = _handle_client_bw(pkt_ptr);
		break;
	//case RTMP_MSG_TYPE_RESERVED07:
	//	break;
	case RTMP_MSG_TYPE_AUDIO:
		LogD(TAG_RTMP, "%s - %s", __FUNCTION__, "RTMP_MSG_TYPE_AUDIO");
		status = _handle_audio(pkt_ptr);
		break;
	case RTMP_MSG_TYPE_VIDEO:
		LogD(TAG_RTMP, "%s - %s", __FUNCTION__, "RTMP_MSG_TYPE_VIDEO");
		status = _handle_video(pkt_ptr);
		break;
	//case RTMP_MSG_TYPE_RESERVED0A:
	//	break;
	//case RTMP_MSG_TYPE_RESERVED0B:
	//	break;
	//case RTMP_MSG_TYPE_RESERVED0C:
	//	break;
	//case RTMP_MSG_TYPE_RESERVED0D:
	//	break;
	//case RTMP_MSG_TYPE_RESERVED0E:
	//	break;
	case RTMP_MSG_TYPE_FLEX_STREAM_SEND:
		LogD(TAG_RTMP, "%s - %s", __FUNCTION__, "RTMP_MSG_TYPE_FLEX_STREAM_SEND");
		status = _handle_flex_stream_send(pkt_ptr);
		break;
	case RTMP_MSG_TYPE_FLEX_SHARED_OBJECT:
		LogD(TAG_RTMP, "%s - %s", __FUNCTION__, "RTMP_MSG_TYPE_FLEX_SHARED_OBJECT");
		status = _handle_flex_shared_object(pkt_ptr);
		break;
	case RTMP_MSG_TYPE_FLEX_MESSAGE:
		LogD(TAG_RTMP, "%s - %s", __FUNCTION__, "RTMP_MSG_TYPE_FLEX_MESSAGE");
		status = _handle_flex_message(pkt_ptr);
		break;
	case RTMP_MSG_TYPE_INFO:
		LogD(TAG_RTMP, "%s - %s", __FUNCTION__, "RTMP_MSG_TYPE_INFO");
		status = _handle_info(pkt_ptr);
		break;
	case RTMP_MSG_TYPE_SHARED_OBJECT:
		LogD(TAG_RTMP, "%s - %s", __FUNCTION__, "RTMP_MSG_TYPE_SHARED_OBJECT");
		status = _handle_shared_object(pkt_ptr);
		break;
	case RTMP_MSG_TYPE_INVOKE:
		LogD(TAG_RTMP, "%s - %s", __FUNCTION__, "RTMP_MSG_TYPE_INVOKE");
		status = _handle_invoke(pkt_ptr);
		break;
	//case RTMP_MSG_TYPE_RESERVED15:
	//	break;
	case RTMP_MSG_TYPE_FLASH_VIDEO:
		LogD(TAG_RTMP, "%s - %s", __FUNCTION__, "RTMP_MSG_TYPE_FLASH_VIDEO");
		status = _handle_flash_video(pkt_ptr);
		break;
	default:
		LogD(TAG_RTMP, "%s - %s", __FUNCTION__, "UNKNOWN");
		status = RT_STATUS_INVALID_PARAMETER;
		break;
	}

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
		if (-1 == _context.socket) {
			status = RT_STATUS_SOCKET_ERR;
			break;
		}

		uint32_t last_timestamp = 0;

		// Optimization with pre-packet
		std::map<uint32_t, rtmp_packet_t>::iterator iter;
		iter = _context.out_channels.find(pkt_ptr->chk_stream_id);
		if (iter != _context.out_channels.end()) {
			rtmp_packet_t pre_packet = iter->second;
			
			if (pkt_ptr->chk_type == RTMP_CHUNK_TYPE_LARGE
				&& pkt_ptr->msg_stream_id == pre_packet.msg_stream_id) {
				pkt_ptr->chk_type == RTMP_CHUNK_TYPE_MEDIUM;
			}
			if (pkt_ptr->chk_type == RTMP_CHUNK_TYPE_MEDIUM
				&& pkt_ptr->msg_type == pre_packet.msg_type
				&& pkt_ptr->valid == pre_packet.valid) {
				pkt_ptr->chk_type = RTMP_CHUNK_TYPE_SMALL;
			}
			if (pkt_ptr->chk_type == RTMP_CHUNK_TYPE_SMALL
				&& pkt_ptr->timestamp == pre_packet.timestamp) { // FIXME: timestamp is all absolute
				pkt_ptr->chk_type = RTMP_CHUNK_TYPE_MINIMUM;
			}

			if (pkt_ptr->chk_type != RTMP_CHUNK_TYPE_LARGE) {
				last_timestamp = pre_packet.timestamp; // Optimized, using delt timestamp
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
				invoke.id = amf_decode_number(ptr);
				invoke.invoke = std::string(content.value.c_str(), content.len);
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
		if (-1 == _context.socket) {
			status = RT_STATUS_SOCKET_ERR;
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

		// Optimization with pre-packet
		rtmp_chunk_type_t chk_type = pkt_ptr->chk_type;
		if (pkt_ptr->chk_type != RTMP_CHUNK_TYPE_LARGE) {
			std::map<uint32_t, rtmp_packet_t>::iterator iter;
			iter = _context.in_channels.find(pkt_ptr->chk_stream_id);
			if (iter != _context.in_channels.end()) {
				rtmp_packet_t pre_packet = iter->second;

				rtmp_copy_packet(pkt_ptr, &pre_packet);
				pkt_ptr->chk_type = chk_type;
			}
		}

		// Read msg header(11/7/3/0) and extend timestamp(4/0)
		uint32_t timestamp = 0;
		uint32_t msg_size = g_msg_header_size[pkt_ptr->chk_type];
		if (!_recv(msg_size, ptr)) {
			status = RT_STATUS_SOCKET_RECV;
			break;
		}
		if (RTMP_CHUNK_TYPE_MINIMUM != pkt_ptr->chk_type) {
			timestamp = amf_decode_u24(ptr);
			ptr += 3;
		}
		if (RTMP_CHUNK_TYPE_MINIMUM != pkt_ptr->chk_type
			&& RTMP_CHUNK_TYPE_SMALL != pkt_ptr->chk_type) {
			pkt_ptr->valid = amf_decode_u24(ptr);
			ptr += 3;
			pkt_ptr->msg_type = (rtmp_msg_type_t)ptr[0];
			ptr += 1;
		}
		if (RTMP_CHUNK_TYPE_LARGE == pkt_ptr->chk_type) {
			pkt_ptr->msg_stream_id = amf_decode_u32le(ptr);
			ptr += 4;
		}
		if (0xFFFFFF == timestamp) {
			if (!_recv(4, ptr)) {
				status = RT_STATUS_SOCKET_RECV;
				break;
			}
			timestamp = amf_decode_u32(ptr);
			ptr += 4;
		}

		// Only large header use absolute timestamp
		if (pkt_ptr->chk_type == RTMP_CHUNK_TYPE_LARGE) {
			pkt_ptr->timestamp = timestamp;
		}
		else {
			pkt_ptr->timestamp += timestamp;
		}

		// Read chunks and composed them to packet
		uint32_t chunk_size = _context.in_chunk_size;
		uint32_t reserved = ptr - buffer;
		uint32_t data_size = pkt_ptr->valid;
		uint8_t *data_ptr = pkt_ptr->data_ptr;
		while (data_size > 0)
		{
			if (data_size < chunk_size)
				chunk_size = data_size;

			if (!_recv(chunk_size, data_ptr)) {
				status = RT_STATUS_SOCKET_RECV;
				break;
			}
			data_ptr += chunk_size;
			data_size -= chunk_size;

			// New header without msg header for splitted chunks
			// Skip following chunk header
			if (data_size > 0) {
				uint32_t reserved1 = reserved - g_msg_header_size[pkt_ptr->chk_type];
				if (!_recv(reserved1, data_ptr)) {
					status = RT_STATUS_SOCKET_RECV;
					break;
				}
			}
		}
		CHECK_BREAK(rt_is_success(status));

		// Update this packet to pre-packet
		_context.in_channels[pkt_ptr->chk_stream_id] = *pkt_ptr;
	} while (false);

	return status;
}



