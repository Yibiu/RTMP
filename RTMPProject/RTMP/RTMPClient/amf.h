#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <memory>
#include "sys_bytes.h"


//////////////////////////////////////////////////////////////////
// Common AMF/AMF3
// AMF is Big-Endian!!!

#define AMF3_INTEGER_MAX	268435455
#define AMF3_INTEGER_MIN	-268435456


typedef enum _amf_data_type
{
	AMF_NUMBER = 0,
	AMF_BOOLEAN,
	AMF_STRING,
	AMF_OBJECT,
	AMF_MOVIECLIP,			// reserved, not used
	AMF_NULL, AMF_UNDEFINED,
	AMF_REFERENCE,
	AMF_ECMA_ARRAY,
	AMF_OBJECT_END,
	AMF_STRICT_ARRAY,
	AMF_DATE,
	AMF_LONG_STRING,
	AMF_UNSUPPORTED,
	AMF_RECORDSET,			// reserved, not used
	AMF_XML_DOC,
	AMF_TYPED_OBJECT,
	AMF_AVMPLUS,			// switch to AMF3
	AMF_INVALID = 0xff
} amf_data_type_t;

typedef enum _amf3_data_type
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
} amf3_data_type_t;

typedef struct _val
{
	char *value;
	int len;
} val_t;
#define AVINIT(str)			{str, sizeof(str) - 1}
#define AVMATCH(a1, a2)		((a1)->len == (a2)->len && 0 == memcmp((a1)->value, (a2)->value, (a1)->len))


//////////////////////////////////////////////////////////////////
// AMF
struct _amf_object_property;
typedef struct _amf_object
{
	uint32_t num;
	_amf_object_property *props;
} amf_object_t;

typedef struct _amf_object_property
{
	val_t name;
	amf_data_type_t type;
	union {
		uint64_t number;
		val_t value;
		amf_object_t object;
	} vu;
	uint16_t UTC_offset;
} amf_object_property_t;

void amf_dump(const amf_object_t *obj);
void amf_add_prop(amf_object_t *obj, const amf_object_property_t *prop);
amf_object_property_t* amf_get_prop(const amf_object_t *obj, const val_t &name, uint32_t index);

// Encode
uint8_t* amf_encode_u16(uint8_t *ptr, uint16_t value);
uint8_t* amf_encode_u24(uint8_t *ptr, uint32_t value);
uint8_t* amf_encode_u32(uint8_t *ptr, uint32_t value);
uint8_t* amf_encode_boolean(uint8_t *ptr, bool value);
uint8_t* amf_encode_string(uint8_t *ptr, const val_t &str);
uint8_t* amf_encode_number(uint8_t *ptr, uint64_t value);
uint8_t* amf_encode_named_string(uint8_t *ptr, const val_t &name, const val_t &value);
uint8_t* amf_encode_named_number(uint8_t *ptr, const val_t &name, uint64_t value);
uint8_t* amf_encode_named_boolean(uint8_t *ptr, const val_t &name, bool value);
uint8_t* amf_encode(uint8_t *ptr, const amf_object_t *obj);
uint8_t* amf_encode_ecma_array(uint8_t *ptr, const amf_object_t *obj);
uint8_t* amf_encode_array(uint8_t *ptr, const amf_object_t *obj);

// Decode
uint16_t amf_decode_u16(const uint8_t *ptr);
uint32_t amf_decode_u24(const uint8_t *ptr);
uint32_t amf_decode_u32(const uint8_t *ptr);
uint32_t amf_decode_u32le(const uint8_t *ptr); // Little endian
bool amf_decode_boolean(const uint8_t *ptr);
void amf_decode_string(const uint8_t *ptr, val_t &str);
void amf_decode_longstring(const uint8_t *ptr, val_t &str);
uint64_t amf_decode_number(const uint8_t *ptr);
int amf_decode(amf_object_t *obj, const uint8_t *ptr, uint32_t size, bool decode_name); // Decode to object, error: -1
int amf_decode_array(amf_object_t *obj, const uint8_t *ptr, uint32_t size, uint32_t array_len, bool decode_name); // Decode array, error: -1

// Property
uint8_t* amfprop_encode(const amf_object_property_t *prop, uint8_t *ptr);
int amfprop_decode(amf_object_property_t *prop, const uint8_t *ptr, uint32_t size, bool decode_name); // Decode to prop, return: skipped bytes or -1 on error
void amfprop_dump(const amf_object_property_t *prop);


//////////////////////////////////////////////////////////////////
// AMF3
typedef struct _amf3_class_def
{
	val_t cd_name;
	char cd_externalizable;
	char cd_dynamic;
	int cd_num;
	val_t *cd_props;
} amf3_class_def_t;

void amf3cd_add_prop(amf3_class_def_t *cd, const val_t &prop);
val_t amf3cd_get_prop(const amf3_class_def_t *cd, int idx);

int amf3_read_integer(const uint8_t *data, int32_t *valp);
int amf3_read_string(const int8_t *data, val_t *str);

int amf3_decode(amf_object_t *obj, const uint8_t *ptr, uint32_t size, bool amf);
int amf3prop_decode(amf_object_property_t *prop, const uint8_t *ptr, uint32_t size, bool decode_name);





