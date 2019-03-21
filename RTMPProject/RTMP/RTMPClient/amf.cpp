#include "amf.h"


static const amf_object_property_t AMFProp_Invalid = { { 0, NULL }, AMF_INVALID };
static const amf_object_t AMFObj_Invalid = { 0, NULL };
static const val_t AV_empty = { 0, NULL };


// AMF
uint8_t* amf_encode_u16(uint8_t *ptr, uint16_t value)
{
	ptr[0] = value >> 8;
	ptr[1] = value & 0xff;
	return ptr + 2;
}

uint8_t* amf_encode_u24(uint8_t *ptr, uint32_t value)
{
	ptr[0] = value >> 16;
	ptr[1] = value >> 8;
	ptr[2] = value & 0xff;
	return ptr + 3;
}

uint8_t* amf_encode_u32(uint8_t *ptr, uint32_t value)
{
	ptr[0] = value >> 24;
	ptr[1] = value >> 16;
	ptr[2] = value >> 8;
	ptr[3] = value & 0xff;
	return ptr + 4;
}

// AMF_BOOLEAN + bool
uint8_t* amf_encode_boolean(uint8_t *ptr, bool value)
{
	*ptr++ = AMF_BOOLEAN;
	*ptr++ = value ? 0x01 : 0x00;
	return ptr;
}

// AMF_STRING/AMF_LONG_STRING + size(2/4 bytes) + string
uint8_t* amf_encode_string(uint8_t *ptr, const val_t &str)
{
	if (str.len < 65536) {
		*ptr++ = AMF_STRING;
		ptr = amf_encode_u16(ptr, str.len);
	}
	else {
		*ptr++ = AMF_LONG_STRING;
		ptr = amf_encode_u32(ptr, str.len);
	}
	memcpy(ptr, str.value, str.len);
	ptr += str.len;

	return ptr;
}

// AMF_NUMBER + number(8 bytes)
uint8_t* amf_encode_number(uint8_t *ptr, uint64_t value)
{
	*ptr++ = AMF_NUMBER;	// type: Number

#if __FLOAT_WORD_ORDER == __BYTE_ORDER
#if __BYTE_ORDER == __BIG_ENDIAN
	memcpy(ptr, &value, 8);
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

// Name size(2 bytes) + name + amf_encode_string(value)
uint8_t* amf_encode_named_string(uint8_t *ptr, const val_t &name, const val_t &value)
{
	ptr = amf_encode_u16(ptr, name.len);
	memcpy(ptr, name.value, name.len);
	ptr += name.len;

	return amf_encode_string(ptr, value);
}

// Name size(2 bytes) + name + amf_encode_number(value)
uint8_t* amf_encode_named_number(uint8_t *ptr, const val_t &name, uint64_t value)
{
	ptr = amf_encode_u16(ptr, name.len);
	memcpy(ptr, name.value, name.len);
	ptr += name.len;

	return amf_encode_number(ptr, value);
}

// Name size(2 bytes) + name + amf_encode_boolean(value)
uint8_t* amf_encode_named_boolean(uint8_t *ptr, const val_t &name, bool value)
{
	ptr = amf_encode_u16(ptr, name.len);
	memcpy(ptr, name.value, name.len);
	ptr += name.len;

	return amf_encode_boolean(ptr, value);
}

/*
char *amf_encode(amf_object_t *obj, char *pBuffer, char *pBufEnd)
{
	int i;

	if (pBuffer + 4 >= pBufEnd)
		return NULL;

	*pBuffer++ = AMF_OBJECT;

	for (i = 0; i < obj->num; i++)
	{
		char *res = amfprop_encode(&obj->props[i], pBuffer, pBufEnd);
		if (res == NULL)
		{
			break;
		}
		else
		{
			pBuffer = res;
		}
	}

	if (pBuffer + 3 >= pBufEnd)
		return NULL;			// no room for the end marker

	pBuffer = amf_encode_u24(pBuffer, pBufEnd, AMF_OBJECT_END);

	return pBuffer;
}

char *amf_encode_ecma_array(amf_object_t *obj, char *pBuffer, char *pBufEnd)
{
	int i;

	if (pBuffer + 4 >= pBufEnd)
		return NULL;

	*pBuffer++ = AMF_ECMA_ARRAY;

	pBuffer = amf_encode_int32(pBuffer, pBufEnd, obj->num);

	for (i = 0; i < obj->num; i++)
	{
		char *res = amfprop_encode(&obj->props[i], pBuffer, pBufEnd);
		if (res == NULL)
		{
			break;
		}
		else
		{
			pBuffer = res;
		}
	}

	if (pBuffer + 3 >= pBufEnd)
		return NULL;			// no room for the end marker

	pBuffer = amf_encode_int24(pBuffer, pBufEnd, AMF_OBJECT_END);

	return pBuffer;
}

char *amf_encode_array(amf_object_t *obj, char *pBuffer, char *pBufEnd)
{
	int i;

	if (pBuffer + 4 >= pBufEnd)
		return NULL;

	*pBuffer++ = AMF_STRICT_ARRAY;

	pBuffer = amf_encode_int32(pBuffer, pBufEnd, obj->num);

	for (i = 0; i < obj->num; i++)
	{
		char *res = amfprop_encode(&obj->props[i], pBuffer, pBufEnd);
		if (res == NULL)
		{
			break;
		}
		else
		{
			pBuffer = res;
		}
	}

	//if (pBuffer + 3 >= pBufEnd)
	//  return NULL;			// no room for the end marker

	//pBuffer = AMF_EncodeInt24(pBuffer, pBufEnd, AMF_OBJECT_END);

	return pBuffer;
}
*/

uint16_t amf_decode_u16(const uint8_t *ptr)
{
	uint16_t value = (ptr[0] << 8) | ptr[1];

	return value;
}

uint32_t amf_decode_u24(const uint8_t *ptr)
{
	uint32_t value = (ptr[0] << 16) | (ptr[1] << 8) | ptr[2];

	return value;
}

uint32_t amf_decode_u32(const uint8_t *ptr)
{
	uint32_t value = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];

	return value;
}

uint32_t amf_decode_u32le(const uint8_t *ptr)
{
	uint32_t value = (ptr[3] << 24) | (ptr[2] << 16) | (ptr[1] << 8) | ptr[0];

	return value;
}

bool amf_decode_boolean(const uint8_t *ptr)
{
	return (0x00 != ptr[0]);
}

void amf_decode_string(const uint8_t *ptr, val_t &str)
{
	str.len = amf_decode_u16(ptr);
	str.value = (str.len > 0) ? (char *)ptr + 2 : NULL;
}

void amf_decode_longstring(const uint8_t *ptr, val_t &str)
{
	str.len = amf_decode_u32(ptr);
	str.value = (str.len > 0) ? (char *)ptr + 4 : NULL;
}

uint64_t amf_decode_number(const uint8_t *ptr)
{
	uint64_t value;
#if __FLOAT_WORD_ORDER == __BYTE_ORDER
#if __BYTE_ORDER == __BIG_ENDIAN
	memcpy(&value, ptr, 8);
#elif __BYTE_ORDER == __LITTLE_ENDIAN
	unsigned char *ci, *co;
	ci = (unsigned char *)ptr;
	co = (unsigned char *)&value;
	co[0] = ci[7];
	co[1] = ci[6];
	co[2] = ci[5];
	co[3] = ci[4];
	co[4] = ci[3];
	co[5] = ci[2];
	co[6] = ci[1];
	co[7] = ci[0];
#endif
#else
#if __BYTE_ORDER == __LITTLE_ENDIAN	// __FLOAT_WORD_ORER == __BIG_ENDIAN
	unsigned char *ci, *co;
	ci = (unsigned char *)ptr;
	co = (unsigned char *)&value;
	co[0] = ci[3];
	co[1] = ci[2];
	co[2] = ci[1];
	co[3] = ci[0];
	co[4] = ci[7];
	co[5] = ci[6];
	co[6] = ci[5];
	co[7] = ci[4];
#else // __BYTE_ORDER == __BIG_ENDIAN && __FLOAT_WORD_ORER == __LITTLE_ENDIAN
	unsigned char *ci, *co;
	ci = (unsigned char *)ptr;
	co = (unsigned char *)&value;
	co[0] = ci[4];
	co[1] = ci[5];
	co[2] = ci[6];
	co[3] = ci[7];
	co[4] = ci[0];
	co[5] = ci[1];
	co[6] = ci[2];
	co[7] = ci[3];
#endif
#endif

	return value;
}

/*
int amf_decode(amf_object_t *obj, const uint8_t *ptr, uint32_t size, bool decode_name)
{
	bool error = false;

	uint32_t orig_size = size;
	obj->num = 0;
	obj->props = NULL;
	while (size > 0)
	{
		amf_object_property_t prop;

		if (size >= 3 && amf_decode_u24(ptr) == AMF_OBJECT_END) {
			size -= 3;
			error = false;
			break;
		}

		int ret = amfprop_decode(&prop, ptr, size, decode_name);
		if (-1 == ret) {
			error = true;
			break;
		}
		else {
			size -= ret;
			if (size < 0) {
				error = true;
				break;
			}
			ptr += ret;
			amf_add_prop(obj, &prop);
		}
	}

	if (error)
		return -1;

	return orig_size - size;
}

int amf_decode_array(amf_object_t *obj, const uint8_t *ptr, uint32_t size, uint32_t array_len, bool decode_name)
{
	bool error = false;

	int orig_size = size;
	obj->num = 0;
	obj->props = NULL;
	while (array_len > 0)
	{
		amf_object_property_t prop;

		array_len--;
		if (size <= 0) {
			error = true;
			break;
		}
		int ret = amfprop_decode(&prop, ptr, size, decode_name);
		if (ret == -1) {
			error = 1;
			break;
		}
		else {
			size -= ret;
			ptr += ret;
			amf_add_prop(obj, &prop);
		}
	}

	if (error)
		return -1;

	return orig_size - size;
}
*/

/*
void amf_dump(amf_object_t *obj)
{
int n;
//RTMP_Log(RTMP_LOGDEBUG, "(object begin)");
for (n = 0; n < obj->num; n++)
{
amfprop_Dump(&obj->props[n]);
}
//RTMP_Log(RTMP_LOGDEBUG, "(object end)");
}

void amf_reset(amf_object_t *obj)
{
int n;
for (n = 0; n < obj->num; n++)
{
amfprop_Reset(&obj->props[n]);
}
free(obj->props);
obj->props = NULL;
obj->num = 0;
}

void amf_add_prop(amf_object_t *obj, const amf_object_property_t *prop)
{
if (!(obj->num & 0x0f))
obj->props = (amf_object_property_t *)realloc(obj->props, (obj->num + 16) * sizeof(amf_object_property_t));
memcpy(&obj->props[obj->num++], prop, sizeof(amf_object_property_t));
}

int amf_count_prop(amf_object_t *obj)
{
return obj->num;
}

amf_object_property_t* amf_get_prop(amf_object_t *obj, const val_t *name, int nIndex)
{
if (nIndex >= 0)
{
if (nIndex < obj->num)
return &obj->props[nIndex];
}
else
{
int n;
for (n = 0; n < obj->num; n++)
{
if (AVMATCH(&obj->props[n].name, name))
return &obj->props[n];
}
}

return (amf_object_property_t *)&AMFProp_Invalid;
}
*/

/*
char *amfprop_encode(amf_object_property_t *prop, char *pBuffer, char *pBufEnd)
{
	if (prop->type == AMF_INVALID)
		return NULL;

	if (prop->type != AMF_NULL && pBuffer + prop->name.len + 2 + 1 >= pBufEnd)
		return NULL;

	if (prop->type != AMF_NULL && prop->name.len)
	{
		*pBuffer++ = prop->name.len >> 8;
		*pBuffer++ = prop->name.len & 0xff;
		memcpy(pBuffer, prop->name.value, prop->name.len);
		pBuffer += prop->name.len;
	}

	switch (prop->type)
	{
	case AMF_NUMBER:
		pBuffer = amf_encode_number(pBuffer, pBufEnd, prop->vu.number);
		break;

	case AMF_BOOLEAN:
		pBuffer = amf_encode_boolean(pBuffer, pBufEnd, prop->vu.number != 0);
		break;

	case AMF_STRING:
		pBuffer = amf_encode_string(pBuffer, pBufEnd, &prop->vu.value);
		break;

	case AMF_NULL:
		if (pBuffer + 1 >= pBufEnd)
			return NULL;
		*pBuffer++ = AMF_NULL;
		break;

	case AMF_OBJECT:
		pBuffer = amf_encode(&prop->vu.object, pBuffer, pBufEnd);
		break;

	case AMF_ECMA_ARRAY:
		pBuffer = amf_encode_ecma_array(&prop->vu.object, pBuffer, pBufEnd);
		break;

	case AMF_STRICT_ARRAY:
		pBuffer = amf_encode_array(&prop->vu.object, pBuffer, pBufEnd);
		break;

	default:
		pBuffer = NULL;
		break;
	};

	return pBuffer;
}

int amfprop_decode(amf_object_property_t *prop, const uint8_t *ptr, uint32_t size, bool decode_name)
{
	uint32_t orig_size = size;

	prop->name.len = 0;
	prop->name.value = NULL;
	if (size == 0 || NULL == ptr)
		return -1;
	if (decode_name && size < 4)
		return -1;

	if (decode_name) {
		uint16_t name_size = amf_decode_u16(ptr);
		if (name_size > size - 2)
			return -1;

		amf_decode_string(ptr, prop->name);
		size -= 2 + name_size;
		ptr += 2 + name_size;
	}
	if (size == 0)
		return -1;

	size--;
	prop->type = (amf_data_type_t)*ptr++;
	switch (prop->type)
	{
	case AMF_NUMBER:
		if (size < 8)
			return -1;
		prop->vu.number = amf_decode_number(ptr);
		size -= 8;
		break;
	case AMF_BOOLEAN:
		if (size < 1)
			return -1;
		prop->vu.number = (double)amf_decode_boolean(ptr);
		size--;
		break;
	case AMF_STRING:
	{
		uint16_t string_size = amf_decode_u16(ptr);
		if (size < (long)string_size + 2)
			return -1;
		amf_decode_string(ptr, prop->vu.value);
		size -= (2 + string_size);
		break;
	}
	case AMF_OBJECT:
	{
		int ret = amf_decode(&prop->vu.object, ptr, size, 1);
		if (ret == -1)
			return -1;
		size -= ret;
		break;
	}
	case AMF_MOVIECLIP:
	{
		return -1;
		break;
	}
	case AMF_NULL:
	case AMF_UNDEFINED:
	case AMF_UNSUPPORTED:
		prop->type = AMF_NULL;
		break;
	case AMF_REFERENCE:
	{
		return -1;
		break;
	}
	case AMF_ECMA_ARRAY:
	{
		size -= 4;

		// next comes the rest, mixed array has a final 0x000009 mark and names, so its an object
		int ret = amf_decode(&prop->vu.object, ptr + 4, size, 1);
		if (ret == -1)
			return -1;
		size -= ret;
		break;
	}
	case AMF_OBJECT_END:
	{
		return -1;
		break;
	}
	case AMF_STRICT_ARRAY:
	{
		uint32_t nArrayLen = amf_decode_u32(ptr);
		size -= 4;
		int ret = amf_decode_array(&prop->vu.object, ptr + 4, size, nArrayLen, 0);
		if (ret == -1)
			return -1;
		size -= ret;
		break;
	}
	case AMF_DATE:
	{
		if (size < 10)
			return -1;

		prop->vu.number = amf_decode_number(ptr);
		prop->UTC_offset = amf_decode_u16(ptr + 8);
		size -= 10;
		break;
	}
	case AMF_LONG_STRING:
	case AMF_XML_DOC:
	{
		uint32_t nStringSize = amf_decode_u32(ptr);
		if (size < (long)nStringSize + 4)
			return -1;
		amf_decode_longstring(ptr, prop->vu.value);
		size -= (4 + nStringSize);
		if (prop->type == AMF_LONG_STRING)
			prop->type = AMF_STRING;
		break;
	}
	case AMF_RECORDSET:
	{
		return -1;
		break;
	}
	case AMF_TYPED_OBJECT:
	{
		return -1;
		break;
	}
	case AMF_AVMPLUS:
	{
		int ret = amf3_decode(&prop->vu.object, ptr, size, 1);
		if (ret == -1)
			return -1;
		size -= ret;
		prop->type = AMF_OBJECT;
		break;
	}
	default:
		return -1;
	}

	return orig_size - size;
}
*/

/*
void amfprop_Dump(amf_object_property_t *prop)
{
	char strRes[256];
	char str[256];
	val_t name;

	if (prop->type == AMF_INVALID)
	{
		return;
	}

	if (prop->type == AMF_NULL)
	{
		return;
	}

	if (prop->name.len)
	{
		name = prop->name;
	}
	else
	{
		name.value = "no-name.";
		name.len = sizeof("no-name.") - 1;
	}
	if (name.len > 18)
		name.len = 18;

	snprintf(strRes, 255, "Name: %18.*s, ", name.len, name.value);

	if (prop->type == AMF_OBJECT)
	{
		amf_dump(&prop->vu.object);
		return;
	}
	else if (prop->type == AMF_ECMA_ARRAY)
	{
		amf_dump(&prop->vu.object);
		return;
	}
	else if (prop->type == AMF_STRICT_ARRAY)
	{
		amf_dump(&prop->vu.object);
		return;
	}

	switch (prop->type)
	{
	case AMF_NUMBER:
		snprintf(str, 255, "NUMBER:\t%.2f", prop->vu.number);
		break;
	case AMF_BOOLEAN:
		snprintf(str, 255, "BOOLEAN:\t%s",
			prop->vu.number != 0.0 ? "TRUE" : "FALSE");
		break;
	case AMF_STRING:
		snprintf(str, 255, "STRING:\t%.*s", prop->vu.value.len,
			prop->vu.value.value);
		break;
	case AMF_DATE:
		snprintf(str, 255, "DATE:\ttimestamp: %.2f, UTC offset: %d",
			prop->vu.number, prop->UTC_offset);
		break;
	default:
		snprintf(str, 255, "INVALID TYPE 0x%02x", (unsigned char)prop->type);
	}

	//RTMP_Log(RTMP_LOGDEBUG, "Property: <%s%s>", strRes, str);
}
*/

// AMF3
/*
int amf3_read_integer(const char *data, int32_t *valp)
{
	int i = 0;
	int32_t val = 0;

	while (i <= 2)
	{				// handle first 3 bytes
		if (data[i] & 0x80)
		{			// byte used
			val <<= 7;		// shift up
			val |= (data[i] & 0x7f);	// add bits
			i++;
		}
		else
		{
			break;
		}
	}

	if (i > 2)
	{				// use 4th byte, all 8bits
		val <<= 8;
		val |= data[3];

		// range check
		if (val > AMF3_INTEGER_MAX)
			val -= (1 << 29);
	}
	else
	{				// use 7bits of last unparsed byte (0xxxxxxx)
		val <<= 7;
		val |= data[i];
	}

	*valp = val;

	return i > 2 ? 4 : i + 1;
}

int amf3_read_string(const char *data, val_t *str)
{
	int32_t ref = 0;
	int len;
	assert(str != 0);

	len = amf3_read_integer(data, &ref);
	data += len;

	if ((ref & 0x1) == 0)
	{				// reference: 0xxx
		uint32_t refIndex = (ref >> 1);
		str->value = NULL;
		str->len = 0;
		return len;
	}
	else
	{
		uint32_t nSize = (ref >> 1);

		str->value = (char *)data;
		str->len = nSize;

		return len + nSize;
	}
	return len;
}
*/

int amf3_decode(amf_object_t *obj, const uint8_t *ptr, uint32_t size, bool amf)
{
	/*
	int nOriginalSize = nSize;
	int32_t ref;
	int len;

	obj->num = 0;
	obj->props = NULL;
	if (bAMFData)
	{
		//if (*pBuffer != AMF3_OBJECT)
		//	RTMP_Log(RTMP_LOGERROR, "AMF3 Object encapsulated in AMF stream does not start with AMF3_OBJECT!");
		pBuffer++;
		nSize--;
	}

	ref = 0;
	len = amf3_read_integer(pBuffer, &ref);
	pBuffer += len;
	nSize -= len;

	if ((ref & 1) == 0)
	{				// object reference, 0xxx
		uint32_t objectIndex = (ref >> 1);

		//RTMP_Log(RTMP_LOGDEBUG, "Object reference, index: %d", objectIndex);
	}
	else				// object instance
	{
		int32_t classRef = (ref >> 1);

		amf3_class_def_t cd = { { 0, 0 }
		};
		amf_object_property_t prop;

		if ((classRef & 0x1) == 0)
		{			// class reference
			uint32_t classIndex = (classRef >> 1);
			//RTMP_Log(RTMP_LOGDEBUG, "Class reference: %d", classIndex);
		}
		else
		{
			int32_t classExtRef = (classRef >> 1);
			int i, cdnum;

			cd.cd_externalizable = (classExtRef & 0x1) == 1;
			cd.cd_dynamic = ((classExtRef >> 1) & 0x1) == 1;

			cdnum = classExtRef >> 2;

			// class name

			len = amf3_read_string(pBuffer, &cd.cd_name);
			nSize -= len;
			pBuffer += len;

			//std::string str = className;

			//RTMP_Log(RTMP_LOGDEBUG,
			//	"Class name: %s, externalizable: %d, dynamic: %d, classMembers: %d",
			//	cd.cd_name.av_val, cd.cd_externalizable, cd.cd_dynamic,
			//	cd.cd_num);

			for (i = 0; i < cdnum; i++)
			{
				val_t memberName;
				if (nSize <= 0)
				{
				invalid:
					//RTMP_Log(RTMP_LOGDEBUG, "%s, invalid class encoding!", __FUNCTION__);
					return nOriginalSize;
				}
				len = amf3_read_string(pBuffer, &memberName);
				//RTMP_Log(RTMP_LOGDEBUG, "Member: %s", memberName.av_val);
				amf3cd_add_prop(&cd, &memberName);
				nSize -= len;
				pBuffer += len;
			}
		}

		// add as referencable object

		if (cd.cd_externalizable)
		{
			int nRes;
			val_t name = AVINIT("DEFAULT_ATTRIBUTE");

			//RTMP_Log(RTMP_LOGDEBUG, "Externalizable, TODO check");

			nRes = amf3prop_decode(&prop, pBuffer, nSize, 0);
			if (nRes == -1) {
				//RTMP_Log(RTMP_LOGDEBUG, "%s, failed to decode AMF3 property!", __FUNCTION__);
			}
			else
			{
				nSize -= nRes;
				pBuffer += nRes;
			}

			amfprop_SetName(&prop, &name);
			amf_add_prop(obj, &prop);
		}
		else
		{
			int nRes, i;
			for (i = 0; i < cd.cd_num; i++)	// non-dynamic
			{
				if (nSize <= 0)
					goto invalid;
				nRes = amf3prop_decode(&prop, pBuffer, nSize, 0);
				if (nRes == -1) {
					//RTMP_Log(RTMP_LOGDEBUG, "%s, failed to decode AMF3 property!", __FUNCTION__);
				}

				amfprop_SetName(&prop, amf3cd_get_prop(&cd, i));
				amf_add_prop(obj, &prop);

				pBuffer += nRes;
				nSize -= nRes;
			}
			if (cd.cd_dynamic)
			{
				int len = 0;

				do
				{
					if (nSize <= 0)
						goto invalid;
					nRes = amf3prop_decode(&prop, pBuffer, nSize, 1);
					amf_add_prop(obj, &prop);

					pBuffer += nRes;
					nSize -= nRes;

					len = prop.name.len;
				} while (len > 0);
			}
		}
		//RTMP_Log(RTMP_LOGDEBUG, "class object!");
	}
	return nOriginalSize - nSize;
	*/

	return 0;
}

/*
void amf3cd_add_prop(amf3_class_def_t *cd, val_t *prop)
{
	if (!(cd->cd_num & 0x0f))
		cd->cd_props = (val_t *)realloc(cd->cd_props, (cd->cd_num + 16) * sizeof(val_t));
	cd->cd_props[cd->cd_num++] = *prop;
}

val_t *amf3cd_get_prop(amf3_class_def_t *cd, int idx)
{
	if (idx >= cd->cd_num)
		return (val_t *)&AV_empty;
	return &cd->cd_props[idx];
}

int amf3prop_decode(amf_object_property_t *prop, const char *pBuffer, int nSize, int bDecodeName)
{
	int nOriginalSize = nSize;
	amf3_data_type_t type;

	prop->name.len = 0;
	prop->name.value = NULL;

	if (nSize == 0 || !pBuffer)
	{
		return -1;
	}

	// decode name
	if (bDecodeName)
	{
		val_t name;
		int nRes = amf3_read_string(pBuffer, &name);

		if (name.len <= 0)
			return nRes;

		nSize -= nRes;
		if (nSize <= 0)
			return -1;
		prop->name = name;
		pBuffer += nRes;
	}

	// decode 
	type = (amf3_data_type_t)*pBuffer++;
	nSize--;

	switch (type)
	{
	case AMF3_UNDEFINED:
	case AMF3_NULL:
		prop->type = AMF_NULL;
		break;
	case AMF3_FALSE:
		prop->type = AMF_BOOLEAN;
		prop->vu.number = 0.0;
		break;
	case AMF3_TRUE:
		prop->type = AMF_BOOLEAN;
		prop->vu.number = 1.0;
		break;
	case AMF3_INTEGER:
	{
		int32_t res = 0;
		int len = amf3_read_integer(pBuffer, &res);
		prop->vu.number = (double)res;
		prop->type = AMF_NUMBER;
		nSize -= len;
		break;
	}
	case AMF3_DOUBLE:
		if (nSize < 8)
			return -1;
		prop->vu.number = amf_decode_number(pBuffer);
		prop->type = AMF_NUMBER;
		nSize -= 8;
		break;
	case AMF3_STRING:
	case AMF3_XML_DOC:
	case AMF3_XML:
	{
		int len = amf3_read_string(pBuffer, &prop->vu.value);
		prop->type = AMF_STRING;
		nSize -= len;
		break;
	}
	case AMF3_DATE:
	{
		int32_t res = 0;
		int len = amf3_read_integer(pBuffer, &res);

		nSize -= len;
		pBuffer += len;

		if ((res & 0x1) == 0)
		{			// reference
			uint32_t nIndex = (res >> 1);
		}
		else
		{
			if (nSize < 8)
				return -1;

			prop->vu.number = amf_decode_number(pBuffer);
			nSize -= 8;
			prop->type = AMF_NUMBER;
		}
		break;
	}
	case AMF3_OBJECT:
	{
		int nRes = amf3_decode(&prop->vu.object, pBuffer, nSize, 1);
		if (nRes == -1)
			return -1;
		nSize -= nRes;
		prop->type = AMF_OBJECT;
		break;
	}
	case AMF3_ARRAY:
	case AMF3_BYTE_ARRAY:
	default:
		return -1;
	}
	if (nSize < 0)
		return -1;

	return nOriginalSize - nSize;
}
*/

