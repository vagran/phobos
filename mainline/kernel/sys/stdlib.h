/*
 * /kernel/sys/stdlib.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#ifndef STDLIB_H_
#define STDLIB_H_
#include <sys.h>
phbSource("$Id$");

#define MAX_CMDLINE_PARAMS	127
extern char *argv[MAX_CMDLINE_PARAMS + 1];
extern int argc;

/* initialize object at predefined location */
#define construct(location, type, ...)	new(location) type(__VA_ARGS__)
void *operator new(size_t size, void *location);

template<class T> void destruct(T *p) { p->~T(); }

#define memset		__builtin_memset
#define memcpy		__builtin_memcpy
#define memmove		__builtin_memmove
#define memcmp		__builtin_memcmp
#define strlen		__builtin_strlen
#define strcpy		__builtin_strcpy

ASMCALL int toupper(int c);
ASMCALL int tolower(int c);

ASMCALL char *strncpy(char *dst, const char *src, u32 len);
ASMCALL int strcmp(const char *s1, const char *s2);
ASMCALL int strncmp(const char *s1, const char *s2, u32 len);
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

ASMCALL i32 strtol(const char *nptr, char **endptr, int base);
ASMCALL u32 strtoul(const char *nptr, char **endptr, int base);
ASMCALL i64 strtoq(const char *nptr, char **endptr, int base);
ASMCALL u64 strtouq(const char *nptr, char **endptr, int base);

u32 gethash(const char *s);
u32 gethash(const u8 *data, u32 size);

typedef void (*PutcFunc)(int c, void *arg);
int kvprintf(const char *fmt, PutcFunc func, void *arg, int radix, va_list ap,
	int maxOut = -1) __format(printf, 1, 0);
int sprintf(char *buf, const char *fmt,...) __format(printf, 2, 3);
int snprintf(char *buf, int bufSize, const char *fmt,...) __format(printf, 3, 4);
int vsprintf(char *buf, const char *fmt, va_list arg) __format(printf, 2, 0);
int vsnprintf(char *buf, int bufSize, const char *fmt, va_list arg) __format(printf, 3, 0);

int sscanf(const char *buf, const char *fmt, ...) __format(scanf, 2, 3);
int vsscanf(const char *buf, char const *fmt, va_list ap) __format(scanf, 2, 0);

#ifdef ENABLE_TRACING
#define trace(fmt,...) printf(fmt, ## __VA_ARGS__)
#else
#define trace(fmt,...)
#endif

#endif /* STDLIB_H_ */
