/*
 * /kernel/sys/types.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#ifndef TYPES_H_
#define TYPES_H_
#include <defs.h>
phbSource("$Id$");

typedef char				i8;
typedef short				i16;
typedef long				i32;
typedef long long			i64;
typedef unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned long		u32;
typedef unsigned long long	u64;

typedef i8		int8_t;
typedef i16		int16_t;
typedef i32		int32_t;
typedef i64		int64_t;
typedef u8		u_int8_t;
typedef u16		u_int16_t;
typedef u32		u_int32_t;
typedef u64		u_int64_t;

#define UCHAR_MAX	0xff
#define	CHAR_MAX	0x7f
#define	CHAR_MIN	(-0x7f - 1)

#define	USHRT_MAX	0xffff
#define	SHRT_MAX	0x7fff
#define	SHRT_MIN	(-0x7fff - 1)

#define	UINT_MAX	0xffffffffU
#define	INT_MAX		0x7fffffff
#define	INT_MIN		(-0x7fffffff - 1)

#define	ULONG_MAX	0xffffffffUL
#define	LONG_MAX	0x7fffffffL
#define	LONG_MIN	(-0x7fffffffL - 1)

#define	ULLONG_MAX	0xffffffffffffffffULL
#define	LLONG_MAX	0x7fffffffffffffffLL
#define	LLONG_MIN	(-0x7fffffffffffffffLL - 1)

#define	UQUAD_MAX	ULLONG_MAX
#define	QUAD_MAX	LLONG_MAX
#define	QUAD_MIN	LLONG_MIN

typedef u32		vaddr_t;
typedef u32		vsize_t;
typedef u64		paddr_t;
typedef u64		psize_t;

#define VSIZE_MAX	((vsize_t)~0)

typedef __SIZE_TYPE__ size_t;

typedef void *Handle;

typedef void (*FUNC_PTR)();

/* variable arguments */
typedef u8* va_list;

#define va_size(arg)		roundup2(sizeof(arg), sizeof(int))
#define va_start(va, arg)	((va) = ((u8 *)&arg) + va_size(arg))
#define va_end(va)
#define va_arg(va, type)	((va) += va_size(type), *(type *)((va) - va_size(type)))

#endif /* TYPES_H_ */
