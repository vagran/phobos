/*
 * /phobos/kernel/gate/gate.S
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
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

	movl	(%esp), %edx /* original stack pointer */
	leal	4(%edx), %esp /* switch stack, drop return address */
	call	*%eax /* call original method */
	pushl	%eax /* return value */

	call	GateObjGetReturnAddress
	pushl	%eax /* return address */
	call	OnUserRet
	popl	%edx /* return address */
	popl	%eax /* return value */
	movl	%esp, %ecx /* user stack pointer */

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
