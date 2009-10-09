/*
 * /kernel/mem/segments.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#ifndef SEGMENTS_H_
#define SEGMENTS_H_
#include <sys.h>
phbSource("$Id$");

class SDT {
public:
	struct SegSelector_s {
		u32	rpl:2,		/* request privilege level */
			ti:1,		/* table index : 0 select gdt, 1 select ldt */
			idx:13;		/* index */
	} __packed;
	typedef struct SegSelector_s SegSelector;

	struct PseudoDescriptor_s {
		u16	limit;
		u32	base;
	} __packed;
	typedef struct PseudoDescriptor_s PseudoDescriptor;

	struct Descriptor_s {
		u32 lolimit:16;		/* segment extent (lsb) */
		u32 lobase:24;		/* segment base address (lsb) */
		u32 type:5;			/* segment type */
		u32 dpl:2;			/* segment descriptor priority level */
		u32 p:1;			/* segment descriptor present */
		u32 hilimit:4;		/* segment extent (msb) */
		u32 unused:2;		/* unused */
		u32 def32:1;		/* default 32 vs 16 bit size */
		u32 gran:1;			/* limit granularity (byte/page units)*/
		u32 hibase:8 ;		/* segment base address  (msb) */
	} __packed;
	typedef struct Descriptor_s Descriptor;

	/* system segments and gate types */
	typedef enum {
		SST_RESERVED1 =		0,	/* reserved */
		SST_TSS16 =			1,	/* 16-bit TSS available */
		SST_LDT =			2,	/* local descriptor table */
		SST_TSS16BSY =		3,	/* 16-bit TSS busy */
		SST_CGT16 =			4,	/* 16-bit call gate */
		SST_TASKGT =		5,	/* task gate */
		SST_IGT16 =			6,	/* 16-bit interrupt gate */
		SST_TGT16 =			7,	/* 16-bit trap gate */
		SST_RESERVED2 =		8,	/* reserved */
		SST_TSS32 =			9,	/* 32-bit TSS available */
		SST_RESERVED3 =		10,	/* reserved */
		SST_TSS32BSY =		11,	/* 32-bit TSS busy */
		SST_CGT32 =			12,	/* 32-bit call gate */
		SST_RESERVED4 =		13,	/* reserved */
		SST_IGT32 =			14,	/* 32-bit interrupt gate */
		SST_TGT32 =			15,	/* 32-bit trap gate */
	} SysSegType;

	/* code- and data-segment descriptor types */
	typedef enum {
		DST_DATA =			0x0,
		DST_CODE =			0x8,
		DST_ACCESSED =		0x1,
		DST_D_WRITE =		0x2,
		DST_D_EXPANDDOWN =	0x4,
		DST_C_READ =		0x2,
		DST_C_CONFORMING =	0x4,
	} DataSegType;
};

#endif /* SEGMENTS_H_ */