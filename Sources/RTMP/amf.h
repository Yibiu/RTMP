#pragma once
#include <stdint.h>
#include <string.h>


#ifdef _WIN32
// Windows is little endian only
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN    4321
#define __BYTE_ORDER __LITTLE_ENDIAN
#define __FLOAT_WORD_ORDER __BYTE_ORDER

#else
#include <sys/param.h>

#if defined(BYTE_ORDER) && !defined(__BYTE_ORDER)
#define __BYTE_ORDER    BYTE_ORDER
#endif

#if defined(BIG_ENDIAN) && !defined(__BIG_ENDIAN)
#define __BIG_ENDIAN	BIG_ENDIAN
#endif

#if defined(LITTLE_ENDIAN) && !defined(__LITTLE_ENDIAN)
#define __LITTLE_ENDIAN	LITTLE_ENDIAN
#endif

#endif

// define default endianness
#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN	1234
#endif

#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN	4321
#endif

#ifndef __BYTE_ORDER
#warning "Byte order not defined on your system, assuming little endian!"
#define __BYTE_ORDER	__LITTLE_ENDIAN
#endif

// ok, we assume to have the same float word order and byte order if float word order is not defined
#ifndef __FLOAT_WORD_ORDER
#warning "Float word order not defined, assuming the same as byte order!"
#define __FLOAT_WORD_ORDER	__BYTE_ORDER
#endif

#if !defined(__BYTE_ORDER) || !defined(__FLOAT_WORD_ORDER)
#error "Undefined byte or float word order!"
#endif

#if __FLOAT_WORD_ORDER != __BIG_ENDIAN && __FLOAT_WORD_ORDER != __LITTLE_ENDIAN
#error "Unknown/unsupported float word order!"
#endif

#if __BYTE_ORDER != __BIG_ENDIAN && __BYTE_ORDER != __LITTLE_ENDIAN
#error "Unknown/unsupported byte order!"
#endif


typedef enum {
	AMF_NUMBER = 0, 
	AMF_BOOLEAN, 
	AMF_STRING, 
	AMF_OBJECT,
	AMF_MOVIECLIP,		// reserved, not used
	AMF_NULL, 
	AMF_UNDEFINED, 
	AMF_REFERENCE, 
	AMF_ECMA_ARRAY, 
	AMF_OBJECT_END,
	AMF_STRICT_ARRAY, 
	AMF_DATE, 
	AMF_LONG_STRING, 
	AMF_UNSUPPORTED,
	AMF_RECORDSET,		// reserved, not used
	AMF_XML_DOC, 
	AMF_TYPED_OBJECT,
	AMF_AVMPLUS,		// switch to AMF3
	AMF_INVALID = 0xff
} AMFDataType;

typedef enum
{
	AMF3_UNDEFINED = 0, 
	AMF3_NULL, 
	AMF3_FALSE, 
	AMF3_TRUE,
	AMF3_INTEGER, 
	AMF3_DOUBLE, 
	AMF3_STRING, 
	AMF3_XML_DOC, 
	AMF3_DATE,
	AMF3_ARRAY, 
	AMF3_OBJECT, 
	AMF3_XML, 
	AMF3_BYTE_ARRAY
} AMF3DataType;


uint8_t* amf_encodeu16(uint8_t *ptr, uint16_t value);
uint8_t* amf_encodeu24(uint8_t *ptr, uint32_t value);
uint8_t* amf_encodeu32(uint8_t *ptr, uint32_t value);
uint8_t* amf_encodestr(uint8_t *ptr, const char *str);
uint8_t* amf_encodenum(uint8_t *ptr, double value);
uint8_t* amf_encodebool(uint8_t *ptr, bool value);
uint8_t* amf_encode_namedstr(uint8_t *ptr, const char *name, const char *value);
uint8_t* amf_encode_namednum(uint8_t *ptr, const char *name, double value);
uint8_t* amf_encode_namedbool(uint8_t *ptr, const char *name, bool value);
