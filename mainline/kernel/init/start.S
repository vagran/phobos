/*
 * /kernel/init/start.S
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <boot.h>

	.text
	.globl start, _start
start:
_start:
	cli
	movl	$boot_stack, %esp
	pushl	%ebx 		/* boot info */
	pushl	%eax		/* boot loader signature */
	call	Bootstrap

/* Multiboot header */
.align	4
/* magic */
.long   MULTIBOOT_HEADER_MAGIC
/* flags */
#define MBH_FLAGS (MULTIBOOT_HF_PAGEALIGN | MULTIBOOT_HF_MEMINFO)
.long   MBH_FLAGS
/* checksum */
.long   -(MULTIBOOT_HEADER_MAGIC + MBH_FLAGS)

	.align 4
	.space	BOOT_STACK_SIZE
	.globl boot_stack
boot_stack:
	.long 0, 0, 0, 0
