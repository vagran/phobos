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

#define PAGE_SHIFT		12
#define PAGE_SIZE		(1 << PAGE_SHIFT)

#define atop(x)			(x >> PAGE_SHIFT)
#define ptoa(x)			(x << PAGE_SHIFT)

#ifndef ASSEMBLER
#include <types.h>
#include <queue.h>
#include <specialreg.h>
#include <frame.h>
#include <stdlib.h>
#include <cpu_instr.h>
#include <mem.h>
#include <dev/device.h>
#include <dev/condev.h>

#ifdef DEBUG
#define assert(x) {if (!(x)) __assert(__FILE__, __LINE__, __STR(x));}
#else /* DEBUG */
#define assert(x)
#endif /* DEBUG */
extern void __assert(const char *file, u32 line, const char *cond);

#define tracec(c) __asm __volatile ("movl $0x3f8, %%edx; outb %%al, %%dx;" \
	: : "a"(c) : "edx")

#ifdef KERNEL

extern int Main(paddr_t firstAddr);
extern void panic(const char *fmt,...) __format(printf, 1, 2) __noreturn;
extern void RunDebugger(const char *fmt,...) __format(printf, 1, 2);

#endif /* KERNEL */

#endif /* ASSEMBLER */

#endif /* SYS_H_ */
