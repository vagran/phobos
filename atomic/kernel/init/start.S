/*
 * /kernel/init/start.S
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#include <boot.h>

	.text
	.code32
	.globl start, _start
start:
_start:
	cli
	movl	$boot_stack, %esp
	pushl	%ebx 		/* boot info */
	pushl	%eax		/* boot loader signature */
	pushl	$start_ret	/* return address to properly align arguments */
	pushl	$Bootstrap
	ret
start_ret:

/* Multiboot header */
.align	4
/* magic */
.long   MULTIBOOT_HEADER_MAGIC
/* flags */
#define MBH_FLAGS (MULTIBOOT_HF_PAGEALIGN | MULTIBOOT_HF_MEMINFO)
.long   MBH_FLAGS
/* checksum */
.long   -(MULTIBOOT_HEADER_MAGIC + MBH_FLAGS)

	.data

	.align 4
	.space	BOOT_STACK_SIZE
	.globl boot_stack
boot_stack:
	.long 0, 0, 0, 0
