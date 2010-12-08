/*
 * /phobos/kernel/lib/String.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

template <class Allocator>
String<Allocator>::String(const char *str) : info(this,
	(StringProvider)&String<Allocator>::Get)
{
	Initialize(str ? strlen(str) + 1 : ALLOC_DEF);
	if (str) {
		(*this) = str;
	}
}

template <class Allocator>
String<Allocator>::String(char c) : info(this,
	(StringProvider)&String<Allocator>::Get)
{
	Initialize(ALLOC_DEF);
	(*this) = c;
}

template <class Allocator>
String<Allocator>::String(Object *obj, StringProvider str, void *arg) : info(this,
	(StringProvider)&String<Allocator>::Get)
{
	Initialize((obj->*str)(0, 0, arg) + 1);
	(obj->*str)(LockBuffer(), bufLen, arg);
	ReleaseBuffer();
}

template <class Allocator>
String<Allocator>::String(const StringInfo &str) : info(this,
	(StringProvider)&String<Allocator>::Get)
{
	Initialize((str.obj->*str.str)(0, 0, str.arg) + 1);
	(str.obj->*str.str)(LockBuffer(), bufLen, str.arg);
	ReleaseBuffer();
}

template <class Allocator>
String<Allocator>::~String()
{
	if (buf && buf != staticBuf) {
		mfree(buf);
	}
}

template <class Allocator>
int
String<Allocator>::Initialize(int len)
{
	lock = 0;
	this->len = 0;
	bufLen = roundup2(len, ALLOC_GRAN);
	if (len <= (int)sizeof(staticBuf)) {
		buf = staticBuf;
		bufLen = sizeof(staticBuf);
	} else {
		buf = (char *)malloc(bufLen);
	}
	if (buf) {
		buf[0] = 0;
		return 0;
	}
	bufLen = 0;
	return -1;
}

template <class Allocator>
int
String<Allocator>::Get(char *buf, int bufLen)
{
	if (!buf) {
		return len;
	}
	strncpy(buf, this->buf, bufLen);
	return len;
}

template <class Allocator>
const String<Allocator> &
String<Allocator>::operator =(const char *str)
{
	if (!str) {
		len = 0;
		buf[0] = 0;
		Realloc(1);
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

template <class Allocator>
const String<Allocator> &
String<Allocator>::operator =(char c)
{
	if (Realloc(sizeof(char) + 1)) {
		return *this;
	}
	buf[0] = c;
	buf[1] = 0;
	len = 1;
	return *this;
}

template <class Allocator>
const String<Allocator> &
String<Allocator>::operator =(const String &str)
{
	len = str.GetLength();
	if (Realloc(len + 1)) {
		return *this;
	}
	memcpy(buf, str.GetBuffer(), len);
	buf[len] = 0;
	return *this;
}

template <class Allocator>
const String<Allocator> &
String<Allocator>::operator =(const StringInfo &str)
{
	len = (str.obj->*str.str)(0, 0, str.arg);
	if (Realloc(len + 1)) {
		return *this;
	}
	(str.obj->*str.str)(buf, len + 1, str.arg);
	return *this;
}

template <class Allocator>
int
String<Allocator>::Compare(const String &str)
{
	return strcmp(buf, str.buf);
}

template <class Allocator>
int
String<Allocator>::Compare(const char *str)
{
	return strcmp(buf, str);
}

template <class Allocator>
int
String<Allocator>::operator ==(const String &str)
{
	return !Compare(str);
}

template <class Allocator>
int
String<Allocator>::operator ==(const char *str)
{
	return !Compare(str);
}

template <class Allocator>
int
String<Allocator>::Realloc(int len)
{
	if (len > bufLen || bufLen - len > ALLOC_HYST) {
		bufLen = roundup2(len, ALLOC_GRAN);
	} else {
		return 0;
	}
	if (bufLen <= (int)sizeof(staticBuf)) {
		if (buf != staticBuf) {
			strncpy(staticBuf, buf, bufLen);
			mfree(buf);
			buf = staticBuf;
		}
		bufLen = sizeof(staticBuf);
	} else {
		if (buf != staticBuf) {
			buf = (char *)mrealloc(buf, bufLen);
		} else {
			buf = (char *)malloc(bufLen);
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

template <class Allocator>
char
String<Allocator>::operator [](int idx)
{
	if (idx >= len) {
		return 0;
	}
	return buf[idx];
}

template <class Allocator>
int
String<Allocator>::Append(const char *str)
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

template <class Allocator>
int
String<Allocator>::Append(char c)
{
	if (Realloc(len + 2)) {
		return -1;
	}
	buf[len++] = c;
	buf[len] = 0;
	return 0;
}

template <class Allocator>
int
String<Allocator>::Append(const String &str)
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

template <class Allocator>
int
String<Allocator>::Append(const StringInfo &str)
{
	int strLen = (str.obj->*str.str)(0, 0, str.arg);
	if (!strLen) {
		return 0;
	}
	if (Realloc(len + strLen + 1)) {
		return -1;
	}
	(str.obj->*str.str)(&buf[len], strLen + 1, str.arg);
	len += strLen;
	return 0;
}

template <class Allocator>
const String<Allocator> &
String<Allocator>::operator +=(const char *str)
{
	Append(str);
	return *this;
}

template <class Allocator>
const String<Allocator> &
String<Allocator>::operator +=(char c)
{
	Append(c);
	return *this;
}

template <class Allocator>
const String<Allocator> &
String<Allocator>::operator +=(const String &str)
{
	Append(str);
	return *this;
}

template <class Allocator>
const String<Allocator> &
String<Allocator>::operator +=(const StringInfo &str)
{
	Append(str);
	return *this;
}

template <class Allocator>
int
String<Allocator>::FormatV(const char *fmt, va_list args)
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

template <class Allocator>
int
String<Allocator>::Format(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	return FormatV(fmt, args);
}

template <class Allocator>
int
String<Allocator>::AppendFormatV(const char *fmt, va_list args)
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

template <class Allocator>
int
String<Allocator>::AppendFormat(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	return AppendFormatV(fmt, args);
}

template <class Allocator>
char *
String<Allocator>::LockBuffer(int len)
{
	if (lock) {
		return 0;
	}
	if (len != -1 && len >= bufLen) {
		if (Realloc(len + 1)) {
			return 0;
		}
	}
	++lock;
	return buf;
}

template <class Allocator>
int
String<Allocator>::ReleaseBuffer(int len)
{
	if (!lock) {
		return -1;
	}
	--lock;
	assert(len == -1 || len < bufLen);
	if (len != -1) {
		buf[len] = 0;
		this->len = len;
	} else {
		this->len = strlen(buf);
		assert(len < bufLen);
	}
	return 0;
}

template <class Allocator>
int
String<Allocator>::Find(char c, int from)
{
	int idx;
	for (idx = from; idx <= len; idx++) {
		if (buf[idx] == c) {
			return idx;
		}
	}
	return -1;
}

template <class Allocator>
int
String<Allocator>::Truncate(int newLen)
{
	if (newLen >= len) {
		return -1;
	}
	len = newLen;
	buf[len] = 0;
	return Realloc(len + 1);
}

template <class Allocator>
int
String<Allocator>::SubStr(String &dst, int start, int end)
{
	if (start >= len || (end != -1 && end <= start)) {
		dst = (char *)0;
		return 0;
	}
	if (end == -1 || end > len) {
		end = len;
	}
	int copyLen = end - start;
	char *pDst = dst.LockBuffer(copyLen);
	memcpy(pDst, &buf[start], copyLen);
	dst.ReleaseBuffer(copyLen);
	return copyLen;
}

template <class Allocator>
int
String<Allocator>::GetToken(String &dst, int start, int *pEnd, const char *sep)
{
	while (start < len) {
		const char *psep = sep;
		while (*psep) {
			if (buf[start] == *psep) {
				break;
			}
			psep++;
		}
		if (!*psep) {
			break;
		}
		start++;
	}
	if (start >= len) {
		dst = (char *)0;
		return 0;
	}
	int end = start + 1;
	while (end < len) {
		const char *psep = sep;
		while (*psep) {
			if (buf[end] == *psep) {
				break;
			}
			psep++;
		}
		if (*psep) {
			break;
		}
		end++;
	}
	int tokLen = end - start;
	assert(tokLen);
	char *pDst = dst.LockBuffer(tokLen);
	memcpy(pDst, &buf[start], tokLen);
	dst.ReleaseBuffer(tokLen);
	if (pEnd) {
		*pEnd = end;
	}
	return tokLen;
}

template <class Allocator>
int
String<Allocator>::Scanf(const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = ScanfV(fmt, ap);
	va_end(ap);
	return(ret);
}

template <class Allocator>
int
String<Allocator>::ScanfV(const char *fmt, va_list args)
{
	return vsscanf(buf, fmt, args);
}

#ifdef KERNEL
/* Instantiate KString */
template class String<KMemAllocator>;
#else /* KERNEL */
#ifndef UNIT_TEST
template class String<UMemAllocator>;
#endif /* UNIT_TEST */
#endif /* KERNEL */
