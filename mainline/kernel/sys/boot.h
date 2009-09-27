/*
 * /kernel/sys/boot.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef BOOT_H_
#define BOOT_H_
#include <sys.h>
phbSource("$Id$");

/* Multiboot specification support */

/* The magic number for the Multiboot header. */
#define MULTIBOOT_HEADER_MAGIC			0x1BADB002

/* The flags for the Multiboot header. */
# define MULTIBOOT_HF_PAGEALIGN			0x00000001
# define MULTIBOOT_HF_MEMINFO			0x00000002
# define MULTIBOOT_HF_VIDEOINFO			0x00000004
# define MULTIBOOT_HF_USEADDRESSES		0x00010000

/* The magic number passed by a Multiboot-compliant boot loader. */
#define MULTIBOOT_BOOTLOADER_MAGIC      0x2BADB002

#endif /* BOOT_H_ */
