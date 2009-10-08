/*
 * /kernel/mem/pte.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef PTE_H_
#define PTE_H_
#include <sys.h>
phbSource("$Id$");

class PTE {
public:
	union PTEntry_s {
		u64		raw;
		struct {
			u64		present:1,
					write:1,
					system:1,
					writeThrough:1,
					cacheDisable:1,
					accessed:1,
					dirty:1,
					PAT:1,
					global:1,
					custom:3,
					physAddr:24,
					reserved:27,
					executeDisable:1;
		} fields __packed;
	} __packed;
	typedef union PTEntry_s PTEntry;

	union PDEntry_s {
		u64		raw;
		struct {
			u64		present:1,
					write:1,
					system:1,
					writeThrough:1,
					cacheDisable:1,
					accessed:1,
					dirty:1,
					zero:1,
					custom:4,
					physAddr:24,
					reserved:27,
					executeDisable:1;
		} fields __packed;
	} __packed;
	typedef union PDEntry_s PDEntry;

	union PDPTEntry_s {
		u64		raw;
		struct {
			u64		present:1,
					reserved1:2,
					writeThrough:1,
					cacheDisable:1,
					reserved2:4,
					custom:3,
					physAddr:24,
					reserved3:28;
		} fields __packed;
	} __packed;
	typedef union PDPTEntry_s PDPTEntry;

	typedef enum {
		F_PRESENT =			0x1,
		F_P =				F_PRESENT,
		F_WRITE =			0x2,
		F_W =				F_WRITE,
		F_SYSTEM =			0x4,
		F_S =				F_SYSTEM,
		F_WRITETHROUGH =	0x8,
		F_WT =				F_WRITETHROUGH,
		F_CACHEDISABLE =	0x10,
		F_CD =				F_CACHEDISABLE,
		F_ACCESSED =		0x20,
		F_A =				F_ACCESSED,
		F_DIRTY =			0x40,
		F_D =				F_DIRTY,
		F_EXECUTEDISABLE =	0x8000000000000000ull,
		F_XD =				F_EXECUTEDISABLE,
	} Flags;

};

#define PD_PAGES		4
#define PT_ENTRIES		(PAGE_SIZE / sizeof(PTE::PTEntry))

#define PG_FRAME		0xffffff000ull

#define PT_SHIFT		12
#define PD_SHIFT		21
#define PDPT_SHIFT		30

#define PT_MASK			(((1 << (PD_SHIFT - PT_SHIFT)) - 1) << PT_SHIFT)
#define PD_MASK			(((1 << (PDPT_SHIFT - PD_SHIFT)) - 1) << PD_SHIFT)
#define PDPT_MASK		(((1 << (32 - PDPT_SHIFT)) - 1) << PT_SHIFT)

#endif /* PTE_H_ */
