/*
 * /phobos/kernel/gate/gate.S
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <mem/mm.h>

/* unified gate entry, gate objects' dispatch tables consist of these entries */

.globl GateEntryStart, GateEntryEnd
GateEntryStart:
	call	1f
1:	popl	%eax		/* pass %eip in %eax */
	/* check CPL */
	movl	%cs, %edx
	andl	$3, %edx
	jz		1f			/* kernel mode */
	movl	%esp, %edx	/* pass %esp in %edx */
	sysenter
1:	movl	$GateKernEntry, %edx
	jmp		*%edx
GateEntryEnd:

#define GATE_ENTRY_SIZE roundup2(GateEntryEnd - GateEntryStart, 0x10)

/* entry point for kernel mode calls */
GateKernEntry:
	subl	$GATE_AREA_ADDRESS, %eax
	xorl	%edx, %edx
	movl	$GATE_ENTRY_SIZE, %ecx
	idivl	%ecx		/* method index in %eax now */
	movl	4(%esp), %edx /* pointer to object */
	pushl	%edx
	pushl	%eax
	pushl	%edx
	call	GateObjGetOrigMethod
	addl	$8, %esp
	call	*%eax
	addl	$4, %esp
	ret
/* Kernel system calls entry point */

.globl GateSysEntry
GateSysEntry:
	movl	$0xed, %eax
