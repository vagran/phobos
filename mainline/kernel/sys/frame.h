/*
 * /kernel/sys/frame.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef FRAME_H_
#define FRAME_H_
#include <sys.h>
phbSource("$Id$");

#define PUSHALL \
	pushl %gs; pushl %fs; pushl %es; pushl %ds; \
	pushl %eax; pushl %ebp; \
	pushl %edi; pushl %esi; pushl %edx; pushl %ecx; pushl %ebx

#define POPALL \
	popl %ebx; popl %ecx; popl %edx; popl %esi; popl %edi; \
	popl %ebp; popl %eax; \
	popl %ds; popl %es; popl %fs; popl %gs;

/* offsets in Frame for assembler */
#define FRAME_EBX		0
#define FRAME_ECX		4
#define FRAME_EDX		8
#define FRAME_ESI		12
#define FRAME_EDI		16
#define FRAME_EBP		20
#define FRAME_EAX		24
#define FRAME_DS		28
#define FRAME_ES		32
#define FRAME_FS		36
#define FRAME_CODE		40
#define FRAME_EIP		44
#define FRAME_CS		48
#define FRAME_EFLAGS	52
#define FRAME_ESP		56
#define FRAME_SS		60

#ifndef ASSEMBLER

typedef struct {
	u32 ebx;
	u32 ecx;
	u32 edx;
	u32 esi;
	u32 edi;
	u32 ebp;
	u32 eax;
	u16 ds, __ds_pad;
	u16 es, __es_pad;
	u16 fs, __fs_pad;
	u16 gs, __gs_pad;
	union
	{
		u32 origeax; /* for syscall frame, auto restart */
		u32 errorcode; /* for trap frame */
		u32 irqno; /* for irq frame */
	} u;
	u32 eip;
	u16 cs, __cs_pad;
	u32 eflags;
	u32 esp;			/* these fields valid only if PL change occured */
	u16 ss, __ss_pad;
} Frame;

#endif /* ASSEMBLER */

#endif /* FRAME_H_ */
