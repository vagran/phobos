/*
 * /phobos/kernel/sys/String.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#ifndef STRING_H_
#define STRING_H_
#include <sys.h>
phbSource("$Id$");

template <class Allocator>
class String : public Object {
public:
	typedef int (Object::*StringProvider)(char *buf, int bufSize);
	struct StringInfo {
		Object *obj;
		StringProvider str;

		inline StringInfo(Object *obj, StringProvider str) {
			this->obj = obj;
			this->str = str;
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
	String(Object *obj, StringProvider str);
	String(const StringInfo &str);
	~String();

	int Get(char *buf, int bufLen);
	StringInfo GetInfo() { return info; }

	const String &operator =(const char *str);
	const String &operator =(char c);
	const String &operator =(const String &str);
	const String &operator =(const StringInfo &str);
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
};

typedef class String<KMemAllocator> KString;

#ifdef KERNEL
#define STRING KString
#else /* KERNEL */
#ifndef UNIT_TEST
#define STRING CString
#endif /* UNIT_TEST */
#endif /* KERNEL */

#define GETSTR(obj, method) STRING::StringInfo(obj, (STRING::StringProvider)&method)

#endif /* STRING_H_ */
