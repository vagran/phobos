/*
 * /phobos/kernel/gate/gate.S
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <frame.h>
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

/* Entry point for kernel mode calls */
GateKernEntry:
	subl	$GATE_AREA_ADDRESS, %eax
	xorl	%edx, %edx
	movl	$GATE_ENTRY_SIZE, %ecx
	idivl	%ecx		/* method index in %eax now */
	movl	4(%esp), %edx /* pointer to object */
	pushl	%eax
	pushl	%edx
	call	GateObjGetOrigMethod
	addl	$8, %esp
	jmp		*%eax

/* Kernel system calls entry point */
.globl GateSysEntry
GateSysEntry:
	movl	$KDATASEL, %ecx
	movl	%ecx, %ds
	movl	%ecx, %es
	sti					/* sysenter clears IF */
	pushl	%edx /* original stack pointer */
	subl	$GATE_AREA_ADDRESS, %eax
	xorl	%edx, %edx
	movl	$GATE_ENTRY_SIZE, %ecx
	idivl	%ecx		/* method index in %eax now */
	movl	(%esp), %edx /* original stack pointer */

	/* validate parameters and get method address */
	pushl	%edx
	pushl	%eax
	call	GateObjValidateCall
	addl	$0x8, %esp
	testl	%eax, %eax
	je		CallFail

	popl	%edx /* original stack pointer */
	movl	%edx, %esp /* switch stack */
	popl	%edx /* return address */
	movl	(%esp), %ecx /* object pointer */
	pushl	%eax /* method address */

	pushl	%edx /* return address */
	pushl	%ecx /* object pointer */
	call	GateObjSetReturnAddress
	addl	$0x8, %esp

	popl	%eax /* method address */
	call	*%eax
	pushl	%eax /* return value */

	call	OnUserRet

	movl	4(%esp), %ecx /* object pointer */
	pushl	%ecx
	call	GateObjGetReturnAddress
	movl	%eax, %edx /* return address */
	movl	4(%esp), %eax /* return value */
	movl	%edx, 4(%esp) /* restore return address in user stack */
	lea		8(%esp), %ecx /* user stack pointer */

	/* restore user segments selectors */
	pushl	%ebx
	movl	$UDATASEL, %ebx
	movl	%ebx, %ds
	movl	%ebx, %es
	xorl	%ebx, %ebx
	movl	%ebx, %fs
	movl	%ebx, %gs
	popl	%ebx

	sysexit /* %edx - user %eip, %ecx - user %esp */

CallFailFile: .asciz __FILE__
CallFailMsg: .asciz "Gate call failed, OnUserRet returned"
CallFail:
	call	OnUserRet /* should not return */
	/* something wrong if it returned */
	pushl	$CallFailMsg
	pushl	$__LINE__
	pushl	$CallFailFile
	call	_panic
