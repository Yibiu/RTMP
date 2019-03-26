#include "rtmp_test.h"


/***************************************************
* Help functions
****************************************************/
char* _put_byte(char *output, uint8_t nVal)
{
	output[0] = nVal;
	return output + 1;
}

char* _put_be16(char *output, uint16_t nVal)
{
	output[1] = nVal & 0xff;
	output[0] = nVal >> 8;
	return output + 2;
}

char* _put_be24(char *output, uint32_t nVal)
{
	output[2] = nVal & 0xff;
	output[1] = nVal >> 8;
	output[0] = nVal >> 16;
	return output + 3;
}

char* _put_be32(char *output, uint32_t nVal)
{
	output[3] = nVal & 0xff;
	output[2] = nVal >> 8;
	output[1] = nVal >> 16;
	output[0] = nVal >> 24;
	return output + 4;
}

char* _put_be64(char *output, uint64_t nVal)
{
	output = _put_be32(output, nVal >> 32);
	output = _put_be32(output, nVal);
	return output;
}

char* _put_amf_string(char *c, const char *str)
{
	uint16_t len = strlen(str);
	c = _put_be16(c, len);
	memcpy(c, str, len);
	return c + len;
}

char* _put_amf_double(char *c, double d)
{
	*c++ = AMF_NUMBER; 
	{
		unsigned char *ci, *co;
		ci = (unsigned char *)&d;
		co = (unsigned char *)c;
		co[0] = ci[7];
		co[1] = ci[6];
		co[2] = ci[5];
		co[3] = ci[4];
		co[4] = ci[3];
		co[5] = ci[2];
		co[6] = ci[1];
		co[7] = ci[0];
	}
	return c + 8;
}


/*********************************************************
* RTMPClient
**********************************************************/
CTestPusher::CTestPusher()
{
	_rtmp_ptr = NULL;
	_manage.fmt_context = NULL;
	_manage.video_stream = NULL;
	_manage.audio_stream = NULL;
	memset(&_metadata, 0x00, sizeof(_metadata));
	_running = false;
	_thread_ptr = NULL;

#ifdef LOG_FILE
	_file_ptr = NULL;
#endif
}

CTestPusher::~CTestPusher()
{
}

bool CTestPusher::create(const char *path_ptr, rtmp_metadata_t &metadata, stream_type_t type)
{
	bool success = false;

	do {
		if (NULL == path_ptr)
			break;

		if (!_init_sockets())
			break;

		av_register_all();
		if (avformat_open_input(&_manage.fmt_context, path_ptr, 0, 0) < 0)
			break;
		if (avformat_find_stream_info(_manage.fmt_context, 0) < 0)
			break;
		if (!_parse_streams(metadata, type))
			break;

		//RTMP_LogSetLevel(RTMP_LOGALL);
#ifdef LOG_FILE
		_file_ptr = fopen("./rtmp.log", "wb+");
		if (NULL == _file_ptr)
			break;
		RTMP_LogSetOutput(_file_ptr);
#endif
		_rtmp_ptr = RTMP_Alloc();
		if (NULL == _rtmp_ptr)
			break;
		RTMP_Init(_rtmp_ptr);

		_metadata = metadata;
		success = true;
	} while (false);

	if (!success) {
		destroy();
	}

	return success;
}

void CTestPusher::destroy()
{
	if (NULL != _rtmp_ptr) {
		RTMP_Free(_rtmp_ptr);
		_rtmp_ptr = NULL;
	}
	_cleanup_sockets();

#ifdef LOG_FILE
	if (NULL != _file_ptr) {
		fclose(_file_ptr);
		_file_ptr = NULL;
	}
#endif
}

bool CTestPusher::connect(const char *url_ptr, uint32_t timeout_secs)
{
	bool success = false;

	do {
		_rtmp_ptr->Link.timeout = timeout_secs;
		_rtmp_ptr->Link.lFlags |= RTMP_LF_LIVE;
		if (RTMP_SetupURL(_rtmp_ptr, (char *)url_ptr) < 0)
			break;

		RTMP_EnableWrite(_rtmp_ptr);
		if (RTMP_Connect(_rtmp_ptr, NULL) < 0)
			break;
		if (RTMP_ConnectStream(_rtmp_ptr, 0) < 0)
			break;

		if (!_send_metadata(_metadata))
			break;

		_running = true;
		_thread_ptr = new std::thread(thread_proc, this);
		if (NULL == _thread_ptr)
			break;

		success = true;
	} while (false);

	if (!success) {
		disconnect();
	}

	return success;
}

void CTestPusher::disconnect()
{
	_running = false;
	if (NULL != _thread_ptr) {
		_thread_ptr->join();
		delete _thread_ptr;
		_thread_ptr = NULL;
	}
	if (NULL != _rtmp_ptr) {
		RTMP_Close(_rtmp_ptr);
	}
}

void CTestPusher::thread_proc_internal()
{
	printf("-------------- Enter Thread ---------------\n");

	_metadata.fps = 0 == _metadata.fps ? 30 : _metadata.fps;
	uint32_t period_ms = 1000 / _metadata.fps;

	uint64_t video_frame_count = 0;
	uint64_t audio_frame_count = 0;
	CTimeStatistics statistics;
	statistics.ResetTimeStatistics();
	statistics.StartTimeMeasurement();
	while (_running)
	{
		uint64_t real_time_ms = statistics.GetDeltaTime();
		uint64_t theory_time_ms = period_ms * video_frame_count;
		if (theory_time_ms > real_time_ms) {
			uint64_t delt_ms = theory_time_ms - real_time_ms;
			Sleep(delt_ms);
		}

		if (!_running)
			break;

		AVPacket pkt = { 0 };
		if (av_read_frame(_manage.fmt_context, &pkt) < 0)
			break;

		// Video frame
		if (pkt.stream_index == _manage.video_stream->index) {
			uint64_t pts = period_ms * video_frame_count;
			bool keyframe = pkt.flags & AV_PKT_FLAG_KEY;
			// Replace size-4-bytes with 0x00,0x00,0x00,0x01
			pkt.data[0] = 0x00;
			pkt.data[1] = 0x00;
			pkt.data[2] = 0x00;
			pkt.data[3] = 0x01;
			if (_send_video(pkt.size, pkt.data, pts, keyframe)) {
				//printf("Send video: frame=%lld, size=%d, pts=%lld, keyframe=%s\n", video_frame_count, 
				//	pkt.size, pts, keyframe ? "true" : "false");
			}
			else {
				printf("Send video: error\n");
			}

			video_frame_count++;
		}
		// Audio frame
		else if (pkt.stream_index == _manage.audio_stream->index) {

			audio_frame_count++;
		}
	}

	printf("-------------- Exit Thread ---------------\n");
}


bool CTestPusher::_init_sockets()
{
	WORD version;
	WSADATA wsaData;
	version = MAKEWORD(1, 1);
	
	return (WSAStartup(version, &wsaData) == 0);
}

void CTestPusher::_cleanup_sockets()
{
	WSACleanup();
}

bool CTestPusher::_parse_streams(rtmp_metadata_t &metadata, stream_type_t type)
{
	for (int i = 0; i < _manage.fmt_context->nb_streams; i++) {
		// Video stream
		if (_manage.fmt_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			_manage.video_stream = _manage.fmt_context->streams[i];
			if (_manage.fmt_context->streams[i]->codecpar->extradata_size > 0) {
				uint32_t size = _manage.fmt_context->streams[i]->codecpar->extradata_size;
				uint8_t *ptr = _manage.fmt_context->streams[i]->codecpar->extradata;

				// Extra data(MP4: avcCfg, H264: raw)
				switch (type)
				{
				case STREAM_FILE_MP4:
				{
					uint32_t offset = 5;
					uint32_t num_sps = ptr[offset++] & 0x1f;
					for (uint32_t j = 0; j < num_sps; j++) {
						metadata.param.size_sps = (ptr[offset++] << 8);
						metadata.param.size_sps |= ptr[offset++];
						memcpy(metadata.param.data_sps, ptr + offset, metadata.param.size_sps);
						offset += metadata.param.size_sps;
					}
					uint32_t num_pps = ptr[offset++];
					for (uint32_t j = 0; j < num_pps; j++) {
						metadata.param.size_pps = (ptr[offset++] << 8);
						metadata.param.size_pps |= ptr[offset++];
						memcpy(metadata.param.data_pps, ptr + offset, metadata.param.size_pps);
						offset += metadata.param.size_pps;
					}
				}
					break;
				case STREAM_H264_RAW:
				{
					uint32_t offset = 0;
					if (ptr[offset] != 0x00 || ptr[offset + 1] != 0x00 || ptr[offset + 2] != 0x00 || ptr[offset + 3] != 0x01) {
						// No valid data...
					}
					else {
						// Find next pos
						offset++;
						while ((ptr[offset] != 0x00 || ptr[offset + 1] != 0x00 || ptr[offset + 2] != 0x00 || ptr[offset + 3] != 0x01) && (offset < size - 3))
							offset++;

						if ((ptr[4] & 0x1f) == 7) { // SPS first
							metadata.param.size_sps = offset - 4;
							memcpy(metadata.param.data_sps, ptr + 4, metadata.param.size_sps);
							metadata.param.size_pps = size - offset - 4;
							memcpy(metadata.param.data_pps, ptr + offset + 4, metadata.param.size_pps);
						}
						else if ((ptr[4] & 0x1f) == 8) { // PPS first
							metadata.param.size_pps = offset - 4;
							memcpy(metadata.param.data_pps, ptr + 4, metadata.param.size_pps);
							metadata.param.size_sps = size - offset - 4;
							memcpy(metadata.param.data_sps, ptr + offset + 4, metadata.param.size_sps);
						}
					}
				}
					break;
				default:
					break;
				}
			}
		}
		// Audio stream
		else if (_manage.fmt_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			_manage.audio_stream = _manage.fmt_context->streams[i];
			if (_manage.fmt_context->streams[i]->codecpar->extradata_size > 0) {
				uint32_t size = _manage.fmt_context->streams[i]->codecpar->extradata_size;
				uint8_t *ptr = _manage.fmt_context->streams[i]->codecpar->extradata;

				// TODO:
				// ...
			}
		}
	}

	return true;
}

bool CTestPusher::_send_metadata(const rtmp_metadata_t &metadata)
{
	if (NULL == _rtmp_ptr || !RTMP_IsConnected(_rtmp_ptr))
		return false;

	RTMPPacket packet;
	RTMPPacket_Alloc(&packet, RTMP_METADATA_SIZE);
	RTMPPacket_Reset(&packet);
	packet.m_packetType = RTMP_PACKET_TYPE_INFO;
	packet.m_nChannel = 0x04;
	packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = _rtmp_ptr->m_stream_id;

	/////////////////////////////////////////////
	// Send media info
	char *ptr = packet.m_body;
	ptr = _put_byte(ptr, AMF_STRING);
	ptr = _put_amf_string(ptr, "@setDataFrame");
	ptr = _put_byte(ptr, AMF_STRING);
	ptr = _put_amf_string(ptr, "onMetaData");
	ptr = _put_byte(ptr, AMF_OBJECT);
	ptr = _put_amf_string(ptr, "copyright");
	ptr = _put_byte(ptr, AMF_STRING);
	ptr = _put_amf_string(ptr, "firehood");
	ptr = _put_amf_string(ptr, "width");
	ptr = _put_amf_double(ptr, metadata.width);
	ptr = _put_amf_string(ptr, "height");
	ptr = _put_amf_double(ptr, metadata.height);
	ptr = _put_amf_string(ptr, "framerate");
	ptr = _put_amf_double(ptr, metadata.fps);
	ptr = _put_amf_string(ptr, "videodatarate");
	ptr = _put_amf_double(ptr, metadata.bitrate_kpbs);
	double vcodec_ID = 7;
	ptr = _put_amf_string(ptr, "videocodecid");
	ptr = _put_amf_double(ptr, vcodec_ID);
	if (metadata.has_audio) {
		ptr = _put_amf_string(ptr, "audiodatarate");
		ptr = _put_amf_double(ptr, metadata.datarate);
		ptr = _put_amf_string(ptr, "audiosamplerate");
		ptr = _put_amf_double(ptr, metadata.samplerate);
		ptr = _put_amf_string(ptr, "audiosamplesize");
		ptr = _put_amf_double(ptr, metadata.samples_per_frame);
		ptr = _put_amf_string(ptr, "stereo");
		ptr = _put_amf_double(ptr, metadata.channels);
		double acodec_ID = 10;
		ptr = _put_amf_string(ptr, "audiocodecid");
		ptr = _put_amf_double(ptr, acodec_ID);
	}
	ptr = _put_amf_string(ptr, "");
	ptr = _put_byte(ptr, AMF_OBJECT_END);
	packet.m_nBodySize = ptr - packet.m_body;
	if (RTMP_SendPacket(_rtmp_ptr, &packet, 0) < 0) {
		RTMPPacket_Free(&packet);
		return false;
	}

	/////////////////////////////////////////////
	// Send decode info
	// FLV video sequence format:
	// Frame type(4 bits) + codecID(4 bits) + AVCPacketType(1 bytes) + CompositionTime
	//	+ AVCDecoderConfiguration
	//
	uint32_t offset = 0;
	packet.m_body[offset++] = 0x17;
	packet.m_body[offset++] = 0x00;
	packet.m_body[offset++] = 0x00;
	packet.m_body[offset++] = 0x00;
	packet.m_body[offset++] = 0x00;
	// AVCDecoderConfiguration  
	packet.m_body[offset++] = 0x01;
	packet.m_body[offset++] = metadata.param.data_sps[1];
	packet.m_body[offset++] = metadata.param.data_sps[2];
	packet.m_body[offset++] = metadata.param.data_sps[3];
	packet.m_body[offset++] = 0xff;
	// SPS
	packet.m_body[offset++] = 0xE1;
	packet.m_body[offset++] = metadata.param.size_sps >> 8;
	packet.m_body[offset++] = metadata.param.size_sps & 0xff;
	memcpy(&packet.m_body[offset], metadata.param.data_sps, metadata.param.size_sps);
	offset += metadata.param.size_sps;
	// PPS
	packet.m_body[offset++] = 0x01;
	packet.m_body[offset++] = metadata.param.size_pps >> 8;
	packet.m_body[offset++] = metadata.param.size_pps & 0xff;
	memcpy(&packet.m_body[offset], metadata.param.data_pps, metadata.param.size_pps);
	offset += metadata.param.size_pps;
	packet.m_packetType = RTMP_PACKET_TYPE_VIDEO;
	packet.m_nBodySize = offset;
	if (RTMP_SendPacket(_rtmp_ptr, &packet, 0) < 0) {
		RTMPPacket_Free(&packet);
		return false;
	}

	RTMPPacket_Free(&packet);
	return true;
}

bool CTestPusher::_send_video(uint32_t size, const uint8_t *data_ptr, uint64_t pts, bool keyframe)
{
	if (NULL == _rtmp_ptr || !RTMP_IsConnected(_rtmp_ptr) || 0 == size || NULL == data_ptr)
		return false;

	RTMPPacket packet;
	RTMPPacket_Alloc(&packet, size + RTMP_RESERVED_HEAD_SIZE * 2);
	RTMPPacket_Reset(&packet);
	packet.m_packetType = RTMP_PACKET_TYPE_VIDEO;
	packet.m_nChannel = 0x04;
	packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet.m_nTimeStamp = pts;
	packet.m_nInfoField2 = _rtmp_ptr->m_stream_id;

	uint32_t offset = 0;
	if (keyframe)
		packet.m_body[offset++] = 0x17;
	else
		packet.m_body[offset++] = 0x27;
	packet.m_body[offset++] = 0x01;
	packet.m_body[offset++] = 0x00;
	packet.m_body[offset++] = 0x00;
	packet.m_body[offset++] = 0x00;
	packet.m_body[offset++] = size >> 24;
	packet.m_body[offset++] = size >> 16;
	packet.m_body[offset++] = size >> 8;
	packet.m_body[offset++] = size & 0xff;
	memcpy(packet.m_body + offset, data_ptr, size);
	packet.m_nBodySize = offset + size;
	if (RTMP_SendPacket(_rtmp_ptr, &packet, 0) < 0) {
		RTMPPacket_Free(&packet);
		return false;
	}

	RTMPPacket_Free(&packet);
	return true;
}

bool CTestPusher::_send_audio(uint32_t size, const uint8_t *data_ptr, uint64_t pts)
{
	if (NULL == _rtmp_ptr || !RTMP_IsConnected(_rtmp_ptr) || 0 == size || NULL == data_ptr)
		return false;

	RTMPPacket packet;
	RTMPPacket_Alloc(&packet, size + RTMP_RESERVED_HEAD_SIZE * 2);
	RTMPPacket_Reset(&packet);
	packet.m_packetType = RTMP_PACKET_TYPE_AUDIO;
	packet.m_nChannel = 0x04;
	packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet.m_nTimeStamp = pts;
	packet.m_hasAbsTimestamp = 0;
	packet.m_nInfoField2 = _rtmp_ptr->m_stream_id;

	packet.m_body[0] = 0xAF;
	packet.m_body[1] = 0x01;
	memcpy(packet.m_body + 2, data_ptr, size);
	packet.m_nBodySize = size + 2;
	if (RTMP_SendPacket(_rtmp_ptr, &packet, FALSE) < 0) {
		RTMPPacket_Free(&packet);
		return false;
	}

	RTMPPacket_Free(&packet);
	return true;
}


// CTestPuller
CTestPuller::CTestPuller()
{
	_rtmp_ptr = NULL;
	_running = false;
	_thread_ptr = NULL;

	_buffer_size = 0;
	_buffer_ptr = NULL;
	_file_ptr = NULL;
}

CTestPuller::~CTestPuller()
{
}

bool CTestPuller::create(const char *path_ptr)
{
	bool success = false;

	do {
		if (NULL == path_ptr)
			break;

		if (!_init_sockets())
			break;

		_rtmp_ptr = RTMP_Alloc();
		if (NULL == _rtmp_ptr)
			break;
		RTMP_Init(_rtmp_ptr);

		_buffer_size = 2 * 1024 * 1024; // 2MB
		_buffer_ptr = new uint8_t[_buffer_size];
		if (NULL == _buffer_ptr)
			break;
		_file_ptr = fopen(path_ptr, "wb+");
		if (NULL == _file_ptr)
			break;

		success = true;
	} while (false);

	if (!success) {
		destroy();
	}

	return success;
}

void CTestPuller::destroy()
{
	if (NULL != _rtmp_ptr) {
		RTMP_Free(_rtmp_ptr);
		_rtmp_ptr = NULL;
	}
	if (NULL != _buffer_ptr) {
		delete[] _buffer_ptr;
		_buffer_ptr = NULL;
	}
	_buffer_size = 0;
	if (NULL != _file_ptr) {
		fclose(_file_ptr);
		_file_ptr = NULL;
	}
	_cleanup_sockets();
}

bool CTestPuller::connect(const char *url_ptr, uint32_t timeout_secs)
{
	bool success = false;

	do {
		_rtmp_ptr->Link.timeout = timeout_secs;
		_rtmp_ptr->Link.lFlags |= RTMP_LF_LIVE;
		if (RTMP_SetupURL(_rtmp_ptr, (char *)url_ptr) < 0)
			break;

		RTMP_SetBufferMS(_rtmp_ptr, 2 * 1024 * 1024); // 2MB
		if (RTMP_Connect(_rtmp_ptr, NULL) < 0)
			break;
		if (RTMP_ConnectStream(_rtmp_ptr, 0) < 0)
			break;

		_running = true;
		_thread_ptr = new std::thread(thread_proc, this);
		if (NULL == _thread_ptr)
			break;

		success = true;
	} while (false);

	if (!success) {
		disconnect();
	}

	return success;
}

void CTestPuller::disconnect()
{
	_running = false;
	if (NULL != _thread_ptr) {
		_thread_ptr->join();
		delete _thread_ptr;
		_thread_ptr = NULL;
	}
	if (NULL != _rtmp_ptr) {
		RTMP_Close(_rtmp_ptr);
	}
}

void CTestPuller::thread_proc_internal()
{
	uint64_t frame_count = 0;

	RTMPPacket packet = { 0 };
	while (_running)
	{
		// Function 1: FLV file
		/*
		int ret = RTMP_Read(_rtmp_ptr, (char *)_buffer_ptr, _buffer_size);
		if (ret < 0)
			break;
		if (ret == 0) // Timeout for recv
			continue;

		fwrite(_buffer_ptr, sizeof(uint8_t), ret, _file_ptr);

		frame_count++;
		if (frame_count % 100 == 0) {
			printf("read frames=%lld\n", frame_count);
		}
		*/

		//
		// Function 2: Parse rtmp stream to h264 and aac
		//
		// url:https://blog.csdn.net/byxdaz/article/details/53993791
		// The FLV header is preplaced with rtmp chunk header, so ignore FLV header before frames!!!
		//
		// Tag + Tag data
		// Tag = Type(1 byte) + length(3 bytes) + timestamp(3/4 bytes) + streamID(3 bytes)
		//
		// Video data = frame type(4 bits) + codecid(4 bits) + AVCPacketType(1 bytes) + CompositionTime(3 bytes)
		//	+ AVCDecoderConfigurationRecord/NALUs
		//
		// Audio data = format(4 bits) + samplerate(2 bits) + depth(1 bit) + type(1 bit) + ACCPacketType(1 byte)
		//	+ AudioObjectType (5 bits) + AudioSpecificConfig/Speex(Fixed)
		//
		uint8_t nalu_header[4] = { 0x00, 0x00, 0x00, 0x01 };

		int ret = RTMP_ReadPacket(_rtmp_ptr, &packet);
		if (ret < 0)
			break;
		if (0 == ret)
			continue;
		if (RTMPPacket_IsReady(&packet)) {
			// Process packet, eg: set chunk size, set bw, ...
			RTMP_ClientPacket(_rtmp_ptr, &packet);

			if (packet.m_packetType == RTMP_PACKET_TYPE_VIDEO) {
				bool keyframe = 0x17 == packet.m_body[0] ? true : false;
				bool sequence = 0x00 == packet.m_body[1];

				printf("keyframe=%s, sequence=%s\n", keyframe ? "true" : "false", sequence ? "true" : "false");

				// SPS/PPS sequence
				if (sequence) {
					uint32_t offset = 10;
					uint32_t sps_num = packet.m_body[offset++] & 0x1f;
					for (int i = 0; i < sps_num; i++) {
						uint8_t ch0 = packet.m_body[offset];
						uint8_t ch1 = packet.m_body[offset + 1];
						uint32_t sps_len = ((ch0 << 8) | ch1);
						offset += 2;
						// Write sps data
						fwrite(nalu_header, sizeof(uint8_t), 4, _file_ptr);
						fwrite(packet.m_body + offset, sizeof(uint8_t), sps_len, _file_ptr);
						offset += sps_len;
					}
					uint32_t pps_num = packet.m_body[offset++] & 0x1f;
					for (int i = 0; i < pps_num; i++) {
						uint8_t ch0 = packet.m_body[offset];
						uint8_t ch1 = packet.m_body[offset + 1];
						uint32_t pps_len = ((ch0 << 8) | ch1);
						offset += 2;
						// Write pps data
						fwrite(nalu_header, sizeof(uint8_t), 4, _file_ptr);
						fwrite(packet.m_body + offset, sizeof(uint8_t), pps_len, _file_ptr);
						offset += pps_len;
					}
				}
				// Nalu frames
				else {
					uint32_t offset = 5;
					uint8_t ch0 = packet.m_body[offset];
					uint8_t ch1 = packet.m_body[offset + 1];
					uint8_t ch2 = packet.m_body[offset + 2];
					uint8_t ch3 = packet.m_body[offset + 3];
					uint32_t data_len = ((ch0 << 24) | (ch1 << 16) | (ch2 << 8) | ch3);
					offset += 4;
					// Write nalu data
					fwrite(nalu_header, sizeof(uint8_t), 4, _file_ptr);
					fwrite(packet.m_body + offset, sizeof(uint8_t),data_len, _file_ptr);
					offset += data_len;
				}
			}
			else if (packet.m_packetType == RTMP_PACKET_TYPE_AUDIO) {
				/*
				bool sequence = 0x00 == packet.m_body[1];

				printf("sequence=%s\n", sequence ? "true" : "false");

				uint8_t format = 0, samplerate = 0, sampledepth = 0, type = 0;
				uint8_t object_type = 0, sample_frequency_index = 0, channels = 0,
					frame_length_flag = 0, depend_on_core_coder = 0, extension_flag = 0;
				// AAC sequence
				if (sequence) {
					format = (packet.m_body[0] & 0xf0) >> 4;
					samplerate = (packet.m_body[0] & 0x0c) >> 2;
					sampledepth = (packet.m_body[0] & 0x02) >> 1;
					type = packet.m_body[0] & 0x01;
					// sequence = packet.m_body[1];
					// AAC(AudioSpecificConfig)
					if (format == 10) {
						uint8_t ch0 = packet.m_body[2];
						uint8_t ch1 = packet.m_body[3];
						uint16_t config = ((ch0 << 8) | ch1);
						object_type = (config & 0xF800) >> 11;
						sample_frequency_index = (config & 0x0780) >> 7;
						channels = (config & 0x78) >> 3;
						frame_length_flag = (config & 0x04) >> 2;
						depend_on_core_coder = (config & 0x02) >> 1;
						extension_flag = config & 0x01;
					}
					// Speex(Fix data here, so no need to parse...)
					else if (format == 11) {
						// 16 KHz, mono, 16bit/sample
						type = 0;
						sampledepth = 1;
						samplerate = 4;
					}
				}
				// Audio frames
				else {
					// ADTS(7 bytes) + AAC data
					uint32_t data_len = packet.m_nBodySize - 2 + 7;
					uint8_t adts[7];
					adts[0] = 0xff;
					adts[1] = 0xf9;
					adts[2] = ((object_type - 1) << 6) | (sample_frequency_index << 2) | (channels >> 2);
					adts[3] = ((channels & 3) << 6) + (data_len >> 11);
					adts[4] = (data_len & 0x7FF) >> 3;
					adts[5] = ((data_len & 7) << 5) + 0x1F;
					adts[6] = 0xfc;
					// Write audio frames
					fwrite(adts, sizeof(uint8_t), 7, _file_ptr);
					fwrite(packet.m_body + 2, sizeof(uint8_t), packet.m_nBodySize - 2, _file_ptr);
				}
				*/
			}
			else if (packet.m_packetType == RTMP_PACKET_TYPE_INFO){
				// TODO:
				// ...
			}
			else {
				// TODO:
				// ...
			}
		}
	}
}


bool CTestPuller::_init_sockets()
{
	WORD version;
	WSADATA wsaData;
	version = MAKEWORD(1, 1);

	return (WSAStartup(version, &wsaData) == 0);
}

void CTestPuller::_cleanup_sockets()
{
	WSACleanup();
}




