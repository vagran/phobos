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

#define atop(x)			((x) >> PAGE_SHIFT)
#define ptoa(x)			((x) << PAGE_SHIFT)

#define tracec(c) ASM ("movl $0x3f8, %%edx; outb %%al, %%dx;" \
	: : "a"(c) : "edx")

#ifndef ASSEMBLER
#include <types.h>
#include <queue.h>
#include <Bitmap.h>
#include <specialreg.h>
#include <frame.h>
#include <stdlib.h>
#include <cpu_instr.h>
#include <bitops.h>

/*
 * The condition in assert is not evaluated in non-DEBUG versions, so
 * be careful when using functions results in conditions.
 */
#define ASSERT(x)	{if (!(x)) __assert(__FILE__, __LINE__, __STR(x));}
#ifdef DEBUG
#define assert(x)	ASSERT(x)
#else /* DEBUG */
#define assert(x)
#endif /* DEBUG */
#define ensure(x)	ASSERT(x)
extern void __assert(const char *file, u32 line, const char *cond);
#define NotReached()	panic("Unreachable code reached")
#define USED(x)		(void)(x)

#include <lock.h>
#include <object.h>
#include <mem.h>
#include <dev/device.h>
#include <dev/chr/syscons.h>
#include <kern/im.h>
#include <kern/tm.h>
#include <dev/sys/cpu.h>
#include <log.h>
#include <kdb/debugger.h>
#include <fs/fs.h>
#include <vfs/vfs.h>
#include <kern/pm.h>

#ifdef KERNEL

extern void Main(paddr_t firstAddr) __noreturn;
extern void RunDebugger(const char *fmt,...) __format(printf, 1, 2);

extern void _panic(const char *fileName, int line, const char *fmt,...) __format(printf, 3, 4) __noreturn;
#define panic(msg,...)	_panic(__FILE__, __LINE__, msg, ## __VA_ARGS__)

/* System default output functions */

void inline vprintf(const char *fmt, va_list va) __format(printf, 1, 0);
void inline vprintf(const char *fmt, va_list va)
{
	if (sysCons) {
		sysCons->VPrintf(fmt, va);
	}
#ifdef DEBUG
	if (sysDebugger) {
		sysDebugger->VTrace(fmt, va);
	}
#endif /* DEBUG */
}

void inline printf(const char *fmt,...)	__format(printf, 1, 2);
void inline printf(const char *fmt,...)
{
	va_list va;
	va_start(va, fmt);
	vprintf(fmt, va);
}

#endif /* KERNEL */

#endif /* ASSEMBLER */

#endif /* SYS_H_ */
