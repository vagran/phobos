/*
 * /kernel/mem/mm.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef MM_H_
#define MM_H_
#include <sys.h>
phbSource("$Id$");

class mm {
public:
	enum {
		QUICKMAP_SIZE =		8 /* pages */
	};
private:
	static vaddr_t firstAddr;
	static PTE::PTEntry *PTmap;
	static PTE::PDEntry *PTD, *PTDpde;
	static PTE::PTEntry *quickMapPTE;

	static inline void FlushTLB() {wcr3(rcr3());}
	static void GrowMem(vaddr_t addr);
public:
	static int isInitialized;

	static void PreInitialize(vaddr_t addr);
	static paddr_t VtoP(vaddr_t va);
	static inline PTE::PDEntry *VtoPDE(vaddr_t va);
	static inline PTE::PTEntry *VtoPTE(vaddr_t va);
	static inline void *OpNew(u32 size);
	static inline void OpDelete(void *p);
	static void *malloc(u32 size, u32 align = 4);
	static void free(void *p);
	static void *QuickMapEnter(paddr_t pa);
	static void QuickMapRemove(vaddr_t va);
};

#define ALLOC(type, count)		((type *)mm::malloc(sizeof(type) * count))

#endif /* MM_H_ */
