#include "logger.h"


CRTMPLogger::CRTMPLogger()
{
	_callback = NULL;
}

CRTMPLogger::~CRTMPLogger()
{
}

void CRTMPLogger::init(rtmp_log_callback callback)
{
	_callback = callback;
}

void CRTMPLogger::log(rtmp_loglevel_t level, const char *fmt, ...)
{
	if (NULL == _callback)
		return;

	// ... -> va_list -> callback
	va_list args;
	va_start(args, fmt);
	_callback(level, fmt, args);
	va_end(args);
}

void CRTMPLogger::log_hex(rtmp_loglevel_t level, uint32_t size, const uint8_t *data_ptr)
{
	if (NULL == _callback)
		return;

	const char hexdig[] = "0123456789abcdef";
	char line[64];
	char *ptr = line;
	int i = 0;
	for (i = 0; i < size; i++) {
		*ptr++ = hexdig[0x0f & (data_ptr[i] >> 4)];
		*ptr++ = hexdig[0x0f & data_ptr[i]];
		if ((i & 0x0f) == 0x0f) {
			*ptr = '\0';
			log(level, "%s", line);
			ptr = line;
		}
		else {
			*ptr++ = ' ';
		}
	}
	if (i & 0x0f) {
		*ptr = '\0';
		log(level, "%s", line);
	}
}
