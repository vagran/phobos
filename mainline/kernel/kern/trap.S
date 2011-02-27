/*
 * /kernel/kern/trap.S
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#include <sys.h>
phbSource("$Id$");

/* Dynamically constructed traps handlers table concept */

#include <frame.h>

.data
.globl TrapTable
TrapTable:
	.long 0

.text

/* unified trap/exception/interrupt entry */
.globl TrapEntry
TrapEntry:
	cli
	pushl	%eax
	movl	$TrapHandler, %eax
	call	*%eax
.globl TrapEntryEnd
TrapEntryEnd:

#define TRAP_ENTRY_SIZE roundup2(TrapEntryEnd - TrapEntry, 0x10)

/* which traps have specific code pushed on stack */
#define TRAP_CODE_MASK		((1 << 8) | (1 << 10) | (1 << 11) | (1 << 12) | \
	(1 << 13) | (1 << 14) | (1 << 17))

TrapHandler:
	popl	%eax
	subl	$(TrapEntryEnd - TrapEntry), %eax
	subl	TrapTable, %eax
	pushl	%edx
	xorl	%edx, %edx
	pushl	%ecx
	movl	$TRAP_ENTRY_SIZE, %ecx
	idivl	%ecx /* now vector index is stored in %eax */
	cmpl	$32, %eax
	jnc		1f
	movl	$TRAP_CODE_MASK, %edx
	btl		%eax, %edx
	jnc		1f
	/* code is pushed */
	popl	%ecx
	popl	%edx
	xchgl	%eax, (%esp) /* vector index to the frame */
	jmp		2f
1: /* no code pushed */
	popl	%ecx
	popl	%edx
	pushl	%eax
	xorl	%eax, %eax /* zero code */
	xchgl	%eax, 4(%esp)
2:
	PUSH_FRAME
	movl	$KDATASEL, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%esp, %eax
	pushl	%eax /* argument for OnTrapRet - frame pointer */
	pushl	%eax /* argument for trap handler - frame pointer */
	/* call high-level handler */
	call	OnTrap
	addl	$4, %esp
	call	OnTrapRet
	addl	$4, %esp
	POP_FRAME
	addl	$8, %esp /* vector index and error code */
	iret
