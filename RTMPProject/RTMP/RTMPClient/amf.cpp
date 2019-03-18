#include "amf.h"


uint8_t* amf_encodeu16(uint8_t *ptr, uint16_t value)
{
	ptr[1] = value & 0xff;
	ptr[0] = value >> 8;
	return (ptr + 2);
}

uint8_t* amf_encodeu24(uint8_t *ptr, uint32_t value)
{
	ptr[2] = value & 0xff;
	ptr[1] = value >> 8;
	ptr[0] = value >> 16;
	return (ptr + 3);
}

uint8_t* amf_encodeu32(uint8_t *ptr, uint32_t value)
{
	ptr[3] = value & 0xff;
	ptr[2] = value >> 8;
	ptr[1] = value >> 16;
	ptr[0] = value >> 24;
	return (ptr + 4);
}

uint8_t* amf_encodestr(uint8_t *ptr, const char *str)
{
	uint32_t len = strlen(str);
	if (len < 65536) {
		*ptr++ = AMF_STRING;
		ptr = amf_encodeu16(ptr, len);
	}
	else {
		*ptr++ = AMF_LONG_STRING;
		ptr = amf_encodeu32(ptr, len);
	}
	memcpy(ptr, str, len);
	ptr += len;
	return ptr;
}

uint8_t* amf_encodenum(uint8_t *ptr, double value)
{
	*ptr++ = AMF_NUMBER;

#if __FLOAT_WORD_ORDER == __BYTE_ORDER
#if __BYTE_ORDER == __BIG_ENDIAN
	memcpy(output, &value, 8);
#elif __BYTE_ORDER == __LITTLE_ENDIAN
	{
		unsigned char *ci, *co;
		ci = (unsigned char *)&value;
		co = (unsigned char *)ptr;
		co[0] = ci[7];
		co[1] = ci[6];
		co[2] = ci[5];
		co[3] = ci[4];
		co[4] = ci[3];
		co[5] = ci[2];
		co[6] = ci[1];
		co[7] = ci[0];
	}
#endif
#else
#if __BYTE_ORDER == __LITTLE_ENDIAN	// __FLOAT_WORD_ORER == __BIG_ENDIAN
	{
		unsigned char *ci, *co;
		ci = (unsigned char *)&value;
		co = (unsigned char *)ptr;
		co[0] = ci[3];
		co[1] = ci[2];
		co[2] = ci[1];
		co[3] = ci[0];
		co[4] = ci[7];
		co[5] = ci[6];
		co[6] = ci[5];
		co[7] = ci[4];
	}
#else // __BYTE_ORDER == __BIG_ENDIAN && __FLOAT_WORD_ORER == __LITTLE_ENDIAN
	{
		unsigned char *ci, *co;
		ci = (unsigned char *)&value;
		co = (unsigned char *)ptr;
		co[0] = ci[4];
		co[1] = ci[5];
		co[2] = ci[6];
		co[3] = ci[7];
		co[4] = ci[0];
		co[5] = ci[1];
		co[6] = ci[2];
		co[7] = ci[3];
	}
#endif
#endif

	return ptr + 8;
}

uint8_t* amf_encodebool(uint8_t *ptr, bool value)
{
	*ptr++ = AMF_BOOLEAN;
	*ptr++ = value ? 0x01 : 0x00;
	return ptr;
}

uint8_t* amf_encode_namedstr(uint8_t *ptr, const char *name, const char *value)
{
	uint32_t len = strlen(name);
	ptr = amf_encodeu16(ptr, len);
	memcpy(ptr, name, len);
	ptr += len;
	return amf_encodestr(ptr, value);
}

uint8_t* amf_encode_namednum(uint8_t *ptr, const char *name, double value)
{
	uint32_t len = strlen(name);
	ptr = amf_encodeu16(ptr, len);
	memcpy(ptr, name, len);
	ptr += len;
	return amf_encodenum(ptr, value);
}

uint8_t* amf_encode_namedbool(uint8_t *ptr, const char *name, bool value)
{
	uint32_t len = strlen(name);
	ptr = amf_encodeu16(ptr, len);
	memcpy(ptr, name, len);
	ptr += len;
	return amf_encodebool(ptr, value);
}
