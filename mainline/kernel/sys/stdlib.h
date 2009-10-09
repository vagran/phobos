/*
 * /kernel/sys/stdlib.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef STDLIB_H_
#define STDLIB_H_
#include <sys.h>
phbSource("$Id$");

ASMCALL void *memset(void *dst, u8 fill, u32 size);
ASMCALL void *memcpy(void *dst, const void *src, u32 size);
ASMCALL void *memmove(void *dst, const void *src, u32 size);
ASMCALL int memcmp(const void *ptr1, const void *ptr2, u32 size);

ASMCALL int toupper(int c);
ASMCALL int tolower(int c);

ASMCALL u32 strlen(const char *str);
ASMCALL char *strcpy(char *dst, const char *src);
ASMCALL int strcmp(const char *s1, const char *s2);
ASMCALL char *strdup(const char *str);

u32 gethash(const char *s);
u32 gethash(u8 *data, u32 size);
u64 gethash64(const char *s);
u64 gethash64(u8 *data, u32 size);

typedef void (*PutcFunc)(int c, void *arg);
int kvprintf(const char *fmt, PutcFunc func, void *arg, int radix, va_list ap);
int sprintf(char *buf, const char *fmt,...);
#define printf(fmt,...)		{if (sysCons) sysCons->Printf(fmt, ## __VA_ARGS__);}
#define vprintf(fmt, va)	{if (sysCons) sysCons->VPrintf(fmt, va);}

typedef enum {
	KLOG_DEBUG,
	KLOG_INFO,
	KLOG_WARNING,
	KLOG_ERROR,
} KLogLevel;

void klog(KLogLevel level, const char *fmt,...);

#endif /* STDLIB_H_ */
