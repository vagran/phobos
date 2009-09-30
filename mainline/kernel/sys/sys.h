/*
 * /kernel/sys/sys.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef SYS_H_
#define SYS_H_
#include <defs.h>
phbSource("$Id$");

#ifndef ASSEMBLER
#include <types.h>
#include <queue.h>
#include <mem.h>
#include <specialreg.h>
#include <cpu_instr.h>

/* variable arguments */
typedef u8* va_list;

#define va_size(arg)		roundup2(sizeof(arg), sizeof(int))
#define va_start(va, arg)	((va) = ((u8 *)&arg) + va_size(arg))
#define va_arg(va, type)	((va) += va_size(type), *(type *)((va) - va_size(type)))

#define panic(x, ...) //temp

#endif /* ASSEMBLER */

#endif /* SYS_H_ */
