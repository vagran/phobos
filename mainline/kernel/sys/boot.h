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

#define BOOT_STACK_SIZE			8192

#ifndef ASSEMBLER

extern u8 _bootbss, _eboot, _btext, _etext, _erodata, _edata, _end,
	_esym, _estabstr, _kernImageEnd, boot_stack;

#endif /* ASSEMBLER */

/* Multiboot specification support */

/* The magic number for the Multiboot header. */
#define MULTIBOOT_HEADER_MAGIC			0x1BADB002

/* The flags for the Multiboot header. */
#define MULTIBOOT_HF_PAGEALIGN			0x00000001
#define MULTIBOOT_HF_MEMINFO			0x00000002
#define MULTIBOOT_HF_VIDEOINFO			0x00000004
#define MULTIBOOT_HF_USEADDRESSES		0x00010000

/* The magic number passed by a Multiboot-compliant boot loader. */
#define MULTIBOOT_BOOTLOADER_MAGIC      0x2BADB002

#ifndef ASSEMBLER

typedef enum {
		MBIF_MEMSIZE =		0x0001,
		MBIF_DEVICE =		0x0002,
		MBIF_CMDLINE =		0x0004,
		MBIF_MODULES =		0x0008,
		MBIF_SYMBOLS =		0x0010,
		MBIF_SECTIONS =		0x0020,
		MBIF_MEMMAP =		0x0040,
		MBIF_DRIVES =		0x0080,
		MBIF_CONFIG =		0x0100,
		MBIF_LOADERNAME =	0x0200,
		MBIF_APMTABLE =		0x0400,
} MBIFlags;

typedef struct {
	u32 flags;
	u32 memLower, memUpper;
	u32 bootDevice;
	char *cmdLine;
	u32 modsCount;
	u32 modsAddr;
} MBInfo;

extern MBInfo *pMBInfo;

#endif /* ASSEMBLER */

#endif /* BOOT_H_ */
