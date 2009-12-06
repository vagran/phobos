/*
 * /kernel/sys/stdlib.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#ifndef STDLIB_H_
#define STDLIB_H_
#include <sys.h>
phbSource("$Id$");

#define MAX_CMDLINE_PARAMS	127
extern char *argv[MAX_CMDLINE_PARAMS + 1];
extern int argc;

#define construct(location, type,...)	new(location) type(__VA_ARGS__)
void *operator new(size_t size, void *location);

ASMCALL void *memset(void *dst, u8 fill, u32 size);
ASMCALL void *memcpy(void *dst, const void *src, u32 size);
ASMCALL void *memmove(void *dst, const void *src, u32 size);
ASMCALL int memcmp(const void *ptr1, const void *ptr2, u32 size);

ASMCALL int toupper(int c);
ASMCALL int tolower(int c);

ASMCALL u32 strlen(const char *str);
ASMCALL char *strcpy(char *dst, const char *src);
ASMCALL int strcmp(const char *s1, const char *s2);
ASMCALL int strncmp(const char *s1, const char *s2, u32 num);
ASMCALL char *strdup(const char *str);
ASMCALL char *strchr(const char *str, int c);

ASMCALL int	isalnum(int c);
ASMCALL int	isalpha(int c);
ASMCALL int	iscntrl(int c);
ASMCALL int	isdigit(int c);
ASMCALL int	isgraph(int c);
ASMCALL int	islower(int c);
ASMCALL int	isprint(int c);
ASMCALL int	ispunct(int c);
ASMCALL int	isspace(int c);
ASMCALL int	isupper(int c);
ASMCALL int	isxdigit(int c);
ASMCALL int	tolower(int c);
ASMCALL int	toupper(int c);
ASMCALL int isascii(int c);

ASMCALL u32 strtoul(const char *nptr, char **endptr, int base);

u32 gethash(const char *s);
u32 gethash(u8 *data, u32 size);
u64 gethash64(const char *s);
u64 gethash64(u8 *data, u32 size);

typedef void (*PutcFunc)(int c, void *arg);
int kvprintf(const char *fmt, PutcFunc func, void *arg, int radix, va_list ap);
int sprintf(char *buf, const char *fmt,...) __format(printf, 2, 3);
#define printf(fmt,...)		{if (sysCons) sysCons->Printf(fmt, ## __VA_ARGS__);}
#define vprintf(fmt, va)	{if (sysCons) sysCons->VPrintf(fmt, va);}

#ifdef ENABLE_TRACING
#define trace(fmt,...) printf(fmt, ## __VA_ARGS__)
#else
#define trace(fmt,...)
#endif

typedef enum {
	KLOG_DEBUG,
	KLOG_INFO,
	KLOG_WARNING,
	KLOG_ERROR,
} KLogLevel;

void klog(KLogLevel level, const char *fmt,...) __format(printf, 2, 3);

#endif /* STDLIB_H_ */
