/*
 * /phobos/kernel/lib/String.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

String::String(const char *str)
{
	Initialize(str ? strlen(str) + 1 : ALLOC_DEF);
	if (str) {
		(*this) = str;
	}
}

String::String(char c)
{
	Initialize(ALLOC_DEF);
	(*this) = c;
}

String::~String()
{
	if (buf && buf != staticBuf) {
		MM::mfree(buf);
	}
}

int
String::Initialize(int len)
{
	this->len = 0;
	bufLen = roundup2(len, ALLOC_GRAN);
	if (len <= (int)sizeof(staticBuf)) {
		buf = staticBuf;
	} else {
		buf = (char *)MM::malloc(bufLen);
	}
	if (buf) {
		buf[0] = 0;
		return 0;
	}
	bufLen = 0;
	return -1;
}

const String &
String::operator =(const char *str)
{
	if (!str) {
		len = 0;
		buf[0] = 0;
		return *this;
	}
	len = strlen(str);
	if (Realloc(len + 1)) {
		return *this;
	}
	memcpy(buf, str, len);
	buf[len] = 0;
	return *this;
}

const String &
String::operator =(char c)
{
	if (Realloc(sizeof(char) + 1)) {
		return *this;
	}
	buf[0] = c;
	buf[1] = 0;
	len = 1;
	return *this;
}

const String &
String::operator =(String &str)
{
	len = str.GetLength();
	if (Realloc(len + 1)) {
		return *this;
	}
	memcpy(buf, str.GetBuffer(), len);
	buf[len] = 0;
	return *this;
}

String::operator char *()
{
	return GetBuffer();
}

int
String::Realloc(int len)
{
	if (len > bufLen || bufLen - len > ALLOC_HYST) {
		bufLen = roundup2(len, ALLOC_GRAN);
	} else {
		return 0;
	}
	if (bufLen <= (int)sizeof(staticBuf)) {
		if (buf != staticBuf) {
			strncpy(staticBuf, buf, bufLen);
			MM::mfree(buf);
			buf = staticBuf;
		}
	} else {
		if (buf != staticBuf) {
			buf = (char *)MM::mrealloc(buf, bufLen);
		} else {
			buf = (char *)MM::malloc(bufLen);
			strncpy(buf, staticBuf, bufLen);
		}
	}
	if (!buf) {
		len = 0;
		bufLen = 0;
		return -1;
	}
	return 0;
}

char
String::operator [](int idx)
{
	if (idx >= len) {
		return 0;
	}
	return buf[idx];
}

int
String::Append(const char *str)
{
	if (!str) {
		return 0;
	}
	int strLen = strlen(str);
	if (!strLen) {
		return 0;
	}
	if (Realloc(len + strLen + 1)) {
		return -1;
	}
	memcpy(&buf[len], str, strLen);
	len += strLen;
	buf[len] = 0;
	return 0;
}

int
String::Append(char c)
{
	if (Realloc(len + 2)) {
		return -1;
	}
	buf[len++] = c;
	buf[len] = 0;
	return 0;
}

int
String::Append(String &str)
{
	int strLen = str.GetLength();
	if (!strLen) {
		return 0;
	}
	if (Realloc(len + strLen + 1)) {
		return -1;
	}
	memcpy(&buf[len], str.GetBuffer(), strLen);
	len += strLen;
	buf[len] = 0;
	return 0;
}

const String &
String::operator +=(const char *str)
{
	Append(str);
	return *this;
}

const String &
String::operator +=(char c)
{
	Append(c);
	return *this;
}

const String &
String::operator +=(String &str)
{
	Append(str);
	return *this;
}

int
String::FormatV(const char *fmt, va_list args)
{
	if (!fmt) {
		len = 0;
		buf[0] = 0;
		return 0;
	}
	int strLen = vsprintf(0, fmt, args);
	if (!strLen) {
		len = 0;
		buf[0] = 0;
		return 0;
	}
	if (Realloc(strLen + 1)) {
		return -1;
	}
	vsnprintf(buf, strLen + 1, fmt, args);
	len = strLen;
	return 0;
}

int
String::Format(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	return FormatV(fmt, args);
}

int
String::AppendFormatV(const char *fmt, va_list args)
{
	if (!fmt) {
		return 0;
	}
	int strLen = vsprintf(0, fmt, args);
	if (!strLen) {
		return 0;
	}
	if (Realloc(len + strLen + 1)) {
		return -1;
	}
	vsnprintf(&buf[len], strLen, fmt, args);
	len += strLen;
	return 0;
}

int
String::AppendFormat(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	return AppendFormatV(fmt, args);
}

char *
String::LockBuffer(int len)
{
	if (lock) {
		return 0;
	}
	if (len != -1 && len > bufLen) {
		if (Realloc(len)) {
			return 0;
		}
	}
	++lock;
	return buf;
}

int
String::ReleaseBuffer(int len)
{
	if (!lock) {
		return -1;
	}
	--lock;
	assert(len == -1 || len <= bufLen);
	if (len != -1 && len == bufLen) {
		if (Realloc(bufLen + 1)) {
			return -1;
		}
	}
	if (len != -1) {
		buf[len] = 0;
		this->len = len;
	} else {
		this->len = strlen(buf);
		assert(len < bufLen);
	}
	return 0;
}
