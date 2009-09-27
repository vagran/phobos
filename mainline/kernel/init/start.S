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
	jmp		boot_entry

/* Multiboot header */
.align	4
/* magic */
.long   MULTIBOOT_HEADER_MAGIC
/* flags */
#define MBH_FLAGS (MULTIBOOT_HF_PAGEALIGN | MULTIBOOT_HF_MEMINFO)
.long   MBH_FLAGS
/* checksum */
.long   -(MULTIBOOT_HEADER_MAGIC + MBH_FLAGS)

boot_entry:
	movl	$0x3f8, %edx
	movl	$'S', %eax
	outb	%al, %dx
	hlt
