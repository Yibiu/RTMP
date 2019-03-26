#include "test_client.h"


CTestPusher::CTestPusher()
{
	_rtmp_ptr = NULL;
	_manage.fmt_context = NULL;
	_manage.video_stream = NULL;
	_manage.audio_stream = NULL;
	memset(&_metadata, 0x00, sizeof(_metadata));
	_running = false;
	_thread_ptr = NULL;
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

		av_register_all();
		if (avformat_open_input(&_manage.fmt_context, path_ptr, 0, 0) < 0)
			break;
		if (avformat_find_stream_info(_manage.fmt_context, 0) < 0)
			break;
		if (!_parse_streams(metadata, type))
			break;

		_rtmp_ptr = new CRTMPClient;
		if (NULL == _rtmp_ptr)
			break;

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
		delete _rtmp_ptr;
		_rtmp_ptr = NULL;
	}
}

bool CTestPusher::connect(const char *url_ptr, uint32_t timeout_secs)
{
	bool success = false;

	do {
		if (NULL == _rtmp_ptr)
			break;

		if (!rt_is_success(_rtmp_ptr->create(url_ptr)))
			break;
		if (!rt_is_success(_rtmp_ptr->connect(timeout_secs)))
			break;

		if (!rt_is_success(_rtmp_ptr->send_medadata(_metadata)))
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
		_rtmp_ptr->disconnect();
		_rtmp_ptr->destroy();
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
			if (rt_is_success(_rtmp_ptr->send_video(pkt.size, pkt.data, pts, keyframe))) {
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

		_rtmp_ptr = new CRTMPClient;
		if (NULL == _rtmp_ptr)
			break;

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
		delete _rtmp_ptr;
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
}

bool CTestPuller::connect(const char *url_ptr, uint32_t timeout_secs)
{
	bool success = false;

	do {
		if (NULL == _rtmp_ptr)
			break;
		if (!rt_is_success(_rtmp_ptr->create(url_ptr)))
			break;
		if (!rt_is_success(_rtmp_ptr->connect(timeout_secs)))
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
		_rtmp_ptr->disconnect();
		_rtmp_ptr->destroy();
	}
}

void CTestPuller::thread_proc_internal()
{
	rtmp_packet_t packet;
	rtmp_init_packet(&packet);
	packet.data_ptr = new uint8_t[RTMP_MAX_CHUNK_SIZE];
	if (NULL == packet.data_ptr)
		return;
	packet.size = RTMP_MAX_CHUNK_SIZE;
	packet.valid = 0;

	while (_running)
	{
		uint8_t nalu_header[4] = { 0x00, 0x00, 0x00, 0x01 };

		rt_status_t status = _rtmp_ptr->recv_packet(&packet);
		if (!rt_is_success(status))
			break;

		// Process packet, eg: set chunk size, set bw, ...
		_rtmp_ptr->handle_packet(&packet);

		if (packet.msg_type == RTMP_MSG_TYPE_VIDEO) {
			bool keyframe = 0x17 == packet.data_ptr[0] ? true : false;
			bool sequence = 0x00 == packet.data_ptr[1];

			printf("keyframe=%s, sequence=%s\n", keyframe ? "true" : "false", sequence ? "true" : "false");

			// SPS/PPS sequence
			if (sequence) {
				uint32_t offset = 10;
				uint32_t sps_num = packet.data_ptr[offset++] & 0x1f;
				for (int i = 0; i < sps_num; i++) {
					uint8_t ch0 = packet.data_ptr[offset];
					uint8_t ch1 = packet.data_ptr[offset + 1];
					uint32_t sps_len = ((ch0 << 8) | ch1);
					offset += 2;
					// Write sps data
					fwrite(nalu_header, sizeof(uint8_t), 4, _file_ptr);
					fwrite(packet.data_ptr + offset, sizeof(uint8_t), sps_len, _file_ptr);
					offset += sps_len;
				}
				uint32_t pps_num = packet.data_ptr[offset++] & 0x1f;
				for (int i = 0; i < pps_num; i++) {
					uint8_t ch0 = packet.data_ptr[offset];
					uint8_t ch1 = packet.data_ptr[offset + 1];
					uint32_t pps_len = ((ch0 << 8) | ch1);
					offset += 2;
					// Write pps data
					fwrite(nalu_header, sizeof(uint8_t), 4, _file_ptr);
					fwrite(packet.data_ptr + offset, sizeof(uint8_t), pps_len, _file_ptr);
					offset += pps_len;
				}
			}
			// Nalu frames
			else {
				uint32_t offset = 5;
				uint8_t ch0 = packet.data_ptr[offset];
				uint8_t ch1 = packet.data_ptr[offset + 1];
				uint8_t ch2 = packet.data_ptr[offset + 2];
				uint8_t ch3 = packet.data_ptr[offset + 3];
				uint32_t data_len = ((ch0 << 24) | (ch1 << 16) | (ch2 << 8) | ch3);
				offset += 4;
				// Write nalu data
				fwrite(nalu_header, sizeof(uint8_t), 4, _file_ptr);
				fwrite(packet.data_ptr + offset, sizeof(uint8_t), data_len, _file_ptr);
				offset += data_len;
			}
		}
		else if (packet.msg_type == RTMP_MSG_TYPE_AUDIO) {
			// TODO:
			// ...
		}
		else if (packet.msg_type == RTMP_MSG_TYPE_INFO){
			// TODO:
			// ...
		}
		else {
			// TODO:
			// ...
		}
	}

	delete[] packet.data_ptr;
}




