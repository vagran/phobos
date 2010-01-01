/*
 * /kernel/dev/sys/cpu_boot.S
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

/* Application Processors booting code */

#define CR0_PE		0x00000001
#define CR0_PG		0x80000000
#define CR4_PAE		0x00000020

#define AP_STACK_SIZE	256

/* this code will be copied from here to the memory accessible from real mode */
.data
.globl APBootEntry
APBootEntry:
	.code16
	cli
	movw	%cs, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss

	/* turn on protected mode */
	movl	%cr0, %eax
	orl		$CR0_PE, %eax
	movl	%eax, %cr0

	/*
	 * Now we need to pass control to C functions. Minimal requirements for that are:
	 * * Set up temporal stack;
	 * * Switch to the kernel GDT and switch code, data and stack segments;
	 * * Enable paging and utilize kernel address space.
	 */

	/* Since stack is shared between all APs, we need to establish lock for its access */
1:	lock btsl $0, APLock - APBootEntry
	pause
	jc		1b

	/*
	 * APBootEntry has offset 0 in real mode, because SIPI vector affects
	 * only code segment selector. So we can use offsets from APBootEntry as
	 * logical addresses.
	 */
	movl	$(APstack - APBootEntry), %esp
	lgdt	APGDTdesc - APBootEntry
	movl	APPDPT - APBootEntry, %eax
	movl	%eax, %cr3 /* Utilize kernel address space */
	movw	APcodeSel - APBootEntry, %ax
	pushl	%eax
	movw	APdataSel - APBootEntry, %dx
	movl	$(code32 - APBootEntry), %eax
	movl	APentryAddr - APBootEntry, %ebx
	addl	%ebx, %eax
	pushl	%eax
	lretl /* switch to 32-bits code segment */
code32:
	.code32
	/* switch data segments */
	movw	%dx, %ds
	movw	%dx, %es
	movw	%dx, %fs
	movw	%dx, %gs
	movw	%dx, %ss
	addl	%ebx, %esp

	/* enable paging */
	movl	%cr4, %eax
	orl		$CR4_PAE, %eax
	movl	%eax, %cr4
	movl	%cr0, %eax
	orl		$CR0_PG, %eax
	movl	%eax, %cr0

	/* and jump to C */
	pushl	$APBootstrap
	ret

	.space AP_STACK_SIZE
APstack: /* temporal stack */
	.long 0, 0, 0, 0
.globl APLock
APLock: /* APs lock, required since they share the stack during initialization */
	.long 0

/* the values below must be initialized before wakening APs */
.globl APentryAddr
APentryAddr:
	.long 0
.globl APGDTdesc
APGDTdesc: /* GDT pseudo-descriptor */
	.space 6
.globl APcodeSel
APcodeSel: /* kernel code segment selector */
	.word 0
.globl APdataSel
APdataSel: /* kernel data segment selector */
	.word 0
.globl APPDPT
APPDPT: /* kernel address space selector (CR3) */
	.long 0

.globl APBootEntryEnd
APBootEntryEnd:
