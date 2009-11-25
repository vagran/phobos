/*
 * /kernel/sys/types.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
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

typedef u32		vaddr_t;
typedef u64		paddr_t;

typedef unsigned int size_t;

/* variable arguments */
typedef u8* va_list;

#define va_roundup2(size, balign)		(((size) + (balign) - 1) & (~((balign) - 1)))
#define va_size(arg)		va_roundup2(sizeof(arg), sizeof(int))
#define va_start(va, arg)	((va) = ((u8 *)&arg) + va_size(arg))
#define va_arg(va, type)	((va) += va_size(type), *(type *)((va) - va_size(type)))

#endif /* TYPES_H_ */
