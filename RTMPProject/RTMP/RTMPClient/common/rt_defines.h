#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <map>
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
// TODO
#endif


#define RTMP_DEFAULT_PORT		1935


typedef enum _rtmp_proto
{
	RTMP_PROTOCOL_RTMP = 0x00,
	RTMP_PROTOCOL_RTMPT	= 0x01,
	RTMP_PROTOCOL_RTMPE = 0x02
} rtmp_proto_t;


typedef struct _rtmp_context
{
	rtmp_proto_t protocol;
	std::string host;
	uint32_t port;
	std::string app;
	std::string stream;
} rtmp_context_t;
