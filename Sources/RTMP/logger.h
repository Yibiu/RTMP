#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>


typedef enum _rtmp_loglevel
{
	RTMP_LOG_INFO = 0,
	RTMP_LOG_WARNING,
	RTMP_LOG_ERROR
} rtmp_loglevel_t;


typedef void(*rtmp_log_callback)(rtmp_loglevel_t level, const char *fmt, va_list);


class CRTMPLogger
{
public:
	CRTMPLogger();
	virtual ~CRTMPLogger();

	void init(rtmp_log_callback callback);
	void log(rtmp_loglevel_t level, const char *fmt, ...);
	void log_hex(rtmp_loglevel_t level, uint32_t size, const uint8_t *data_ptr);

protected:
	rtmp_log_callback _callback;
};
