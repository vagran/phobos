/*
 * /kernel/sys/types.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#ifndef TYPES_H_
#define TYPES_H_
#include <defs.h>
phbSource("$Id$");

typedef char				i8;
typedef short				i16;
typedef int					i32;
typedef long				i64;
typedef unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned int		u32;
typedef unsigned long		u64;

typedef i8		int8_t;
typedef i16		int16_t;
typedef i32		int32_t;
typedef i64		int64_t;
typedef u8		u_int8_t;
typedef u16		u_int16_t;
typedef u32		u_int32_t;
typedef u64		u_int64_t;

#if (__SIZEOF_POINTER__ == 8)
typedef i64			intptr_t;
typedef u64			uintptr_t;
#else
typedef i32			intptr_t;
typedef u32			uintptr_t;
#endif

#define UCHAR_MAX	0xff
#define	CHAR_MAX	0x7f
#define	CHAR_MIN	(-CHAR_MAX - 1)

#define	USHRT_MAX	0xffff
#define	SHRT_MAX	0x7fff
#define	SHRT_MIN	(-SHRT_MAX - 1)

#define	UINT_MAX	0xffffffffU
#define	INT_MAX		0x7fffffff
#define	INT_MIN		(-INT_MAX - 1)

#define	ULONG_MAX	0xffffffffffffffffUL
#define	LONG_MAX	0x7fffffffffffffffL
#define	LONG_MIN	(-LONG_MAX - 1L)

#define	UQUAD_MAX	ULONG_MAX
#define	QUAD_MAX	LONG_MAX
#define	QUAD_MIN	LONG_MIN

typedef uintptr_t		vaddr_t;
typedef uintptr_t		vsize_t;
typedef u64				paddr_t;
typedef u64				psize_t;

#define VSIZE_MAX	((vsize_t)~0)

typedef __SIZE_TYPE__ size_t;

typedef void *Handle;

typedef uintptr_t	waitid_t;

typedef void (*FUNC_PTR)();

/* variable arguments */
typedef u8* va_list;

#define va_size(arg)		roundup2(sizeof(arg), sizeof(int))
#define va_start(va, arg)	((va) = ((u8 *)&arg) + va_size(arg))
#define va_end(va)
#define va_arg(va, type)	((va) += va_size(type), *(type *)((va) - va_size(type)))

#endif /* TYPES_H_ */
