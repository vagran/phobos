/*
 * /phobos/kernel/sys/String.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#ifndef STRING_H_
#define STRING_H_
#include <sys.h>
phbSource("$Id$");

template <class Allocator>
class String : public Object {
public:
	typedef int (Object::*StringProvider)(char *buf, int bufSize, void *arg);
	struct StringInfo {
		Object *obj;
		StringProvider str;
		void *arg;

		inline StringInfo(Object *obj, StringProvider str, void *arg = 0) {
			this->obj = obj;
			this->str = str;
			this->arg = arg;
		}
	};
private:
	enum {
		ALLOC_DEF =		64, /* default allocation size */
		ALLOC_GRAN =	16, /* allocation granularity */
		ALLOC_HYST =	128, /* allocation hysteresis */
	};
	char staticBuf[ALLOC_DEF];
	char *buf;
	int len, bufLen;
	int lock;
	Allocator alloc;
	StringInfo info;

	inline void *malloc(u32 size) { return alloc.malloc(size); }
	inline void *mrealloc(void *p, u32 size) { return alloc.mrealloc(p, size); }
	inline void mfree(void *p) { alloc.mfree(p); }
	int Initialize(int len);
	int Realloc(int len);
public:
	String(const char *str = 0);
	String(char c);
	String(Object *obj, StringProvider str, void *arg = 0);
	String(const StringInfo &str);
	~String();

	int Get(char *buf, int bufLen);
	StringInfo GetInfo() { return info; }

	const String &operator =(const char *str);
	const String &operator =(char c);
	const String &operator =(const String &str);
	const String &operator =(const StringInfo &str);
	int Compare(const String &str);
	int Compare(const char *str);
	int operator ==(const String &str);
	int operator ==(const char *str);
	inline char *GetBuffer() const { return buf; }
	inline operator char *() { return buf; }
	inline operator StringInfo &() { return info; }
	inline int GetLength() const { return len; }
	char operator [](int idx);
	int Append(const char *str);
	int Append(char c);
	int Append(const String &str);
	int Append(const StringInfo &str);
	const String &operator +=(const char *str);
	const String &operator +=(char c);
	const String &operator +=(const String &str);
	const String &operator +=(const StringInfo &str);
	int FormatV(const char *fmt, va_list args) __format(printf, 2, 0);
	int Format(const char *fmt, ...) __format(printf, 2, 3);
	int AppendFormatV(const char *fmt, va_list args) __format(printf, 2, 0);
	int AppendFormat(const char *fmt, ...) __format(printf, 2, 3);
	char *LockBuffer(int len = -1);
	int ReleaseBuffer(int len = -1);
	int Find(char c, int from = 0); /* return index of the first occurrence, -1 if not found */
	int Truncate(int newLen);
	inline void Empty() { *this = (char *)0; }
	/* return number of characters in the result */
	int SubStr(String &dst, int start, int end = -1);
	/* return number of characters in found token, 0 if not found */
	int GetToken(String &dst, int start, int *pEnd = 0, const char *sep = " \t");
	int Scanf(const char *fmt, ...) __format(scanf, 2, 3);
	int ScanfV(const char *fmt, va_list args) __format(scanf, 2, 0);
};

typedef class String<KMemAllocator> KString;

#ifdef KERNEL
#define STRING KString
#else /* KERNEL */
#ifndef UNIT_TEST
#define STRING CString
#endif /* UNIT_TEST */
#endif /* KERNEL */

/* Define string provider function/method */
#define DECL_STR_PROV(name) int name(char *buf, int bufLen, void *arg = 0)
#define DEF_STR_PROV(name) int name(char *buf, int bufLen, void *arg)

#define GETSTR(obj, method, ...) STRING::StringInfo(obj, \
	(STRING::StringProvider)&method, ## __VA_ARGS__)

#endif /* STRING_H_ */
