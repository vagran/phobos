/*
 * /kernel/sys/frame.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#ifndef FRAME_H_
#define FRAME_H_
#include <sys.h>
phbSource("$Id$");

#define PUSH_FRAME \
	pushl %gs; pushl %fs; pushl %es; pushl %ds; \
	pushl %eax; pushl %ebp; \
	pushl %edi; pushl %esi; pushl %edx; pushl %ecx; pushl %ebx

#define POP_FRAME \
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
#define FRAME_GS		40
#define FRAME_IDX		44
#define FRAME_CODE		48
#define FRAME_EIP		52
#define FRAME_CS		56
#define FRAME_EFLAGS	60
#define FRAME_ESP		64
#define FRAME_SS		68

/* can not use symbolic names since these macros are used in assembler */
#define KCODESEL		(1 << 3) /* SI_KCODE */
#define KDATASEL		(2 << 3) /* SI_KDATA */
#define UCODESEL		((3 << 3) | 3) /* SI_UCODE */
#define UDATASEL		((4 << 3) | 3) /* SI_UDATA */

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
	u32 vectorIdx;
	/* below is CPU hardware assisted */
	u32 code; /* error code for trap frame, zero if not used */
	u32 eip;
	u16 cs, __cs_pad;
	u32 eflags;
	/* below is present if transition rings */
	u32 esp;
	u16 ss, __ss_pad;
} Frame;

#endif /* ASSEMBLER */

#endif /* FRAME_H_ */
