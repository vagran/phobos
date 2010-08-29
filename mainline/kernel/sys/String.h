/*
 * /phobos/kernel/sys/String.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef STRING_H_
#define STRING_H_
#include <sys.h>
phbSource("$Id$");

class String : public Object {
private:
	enum {
		ALLOC_DEF =		64, /* default allocation size */
		ALLOC_GRAN =	16, /* allocation granularity */
		ALLOC_HYST =	128, /* allocation hysteresis */
	};
	char staticBuf[ALLOC_DEF];
	char *buf;
	int len, bufLen;
	AtomicInt<u32> lock;

	int Initialize(int len);
	int Realloc(int len);
public:
	String(const char *str = 0);
	String(char c);
	~String();

	const String &operator =(const char *str);
	const String &operator =(char c);
	const String &operator =(String &str);
	char *GetBuffer() { return buf; }
	operator char *();
	inline int GetLength() { return len; }
	char operator [](int idx);
	int Append(const char *str);
	int Append(char c);
	int Append(String &str);
	const String &operator +=(const char *str);
	const String &operator +=(char c);
	const String &operator +=(String &str);
	int FormatV(const char *fmt, va_list args) __format(printf, 2, 0);
	int Format(const char *fmt, ...) __format(printf, 2, 3);
	int AppendFormatV(const char *fmt, va_list args) __format(printf, 2, 0);
	int AppendFormat(const char *fmt, ...) __format(printf, 2, 3);
	char *LockBuffer(int len = -1);
	int ReleaseBuffer(int len = -1);
};

#endif /* STRING_H_ */
