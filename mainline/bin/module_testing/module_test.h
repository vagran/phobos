/*
 * /phobos/bin/module_testing/module_test.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#ifndef MODULE_TEST_H_
#define MODULE_TEST_H_
#include <sys.h>
phbSource("$Id$");

enum {
	MT_BYTE_VALUE =		42, /* The meaning of the life */
	MT_BYTE_VALUE2 =	(u8)~MT_BYTE_VALUE,
	MT_WORD_VALUE =		0x1234,
	MT_WORD_VALUE2 =	(u16)~MT_WORD_VALUE,
	MT_DWORD_VALUE =	0x13245768,
	MT_DWORD_VALUE2 =	~0x13245768,
};

inline void __mt_logv(const char *func, u32 line, const char *fmt, va_list args)
	__format(printf, 3, 0);
inline void
__mt_logv(const char *func, u32 line, const char *fmt, va_list args)
{
	CString s = "Module testing: ";
	s.AppendFormatV(fmt, args);
	printf("%s\n", s.GetBuffer());
}

inline void __mt_log(const char *func, u32 line, const char *fmt,...)
	__format(printf, 3, 4);
inline void
__mt_log(const char *func, u32 line, const char *fmt,...)
{
	va_list args;
	va_start(args, fmt);
	__mt_logv(func, line, fmt, args);
}

#define mtlog(fmt, ...) __mt_log(__func__, __LINE__, fmt, ## __VA_ARGS__)

#define mtlogv(fmt, args) _klogv(__func__, __LINE__, fmt, args)

inline void
__mt_assert(const char *file, u32 line, const char *cond)
{
	mtlog("Module testing assert failed at '%s@%lu': '%s'", file, line, cond);
	uLib->GetApp()->ExitThread(MT_DWORD_VALUE2);
}

#define mt_assert(condition) { if (!condition) \
	__mt_assert(__FILE__, __LINE__, __STR(condition)); }

#endif /* MODULE_TEST_H_ */
