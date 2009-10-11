/*
 * /kernel/mem/mm.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <mem.h>

paddr_t IdlePDPT, IdlePTD;

vaddr_t quickMap;

vaddr_t mm::firstAddr = 0;

int mm::isInitialized = 0;

PTE::PTEntry *mm::PTmap = (PTE::PTEntry *)((vaddr_t)-PD_PAGES * PT_ENTRIES * PAGE_SIZE);

PTE::PDEntry *mm::PTD = (PTE::PDEntry *)((vaddr_t)-PD_PAGES * PAGE_SIZE);

PTE::PDEntry *mm::PTDpde = (PTE::PDEntry *)((vaddr_t)-PD_PAGES * sizeof(PTE::PDEntry));

PTE::PTEntry *mm::quickMapPTE;

void
mm::PreInitialize(vaddr_t addr)
{
	firstAddr = addr;

	/*
	 * Create page tables recursive mapping on the top of VM space.
	 * Use identity mapping to modify PTD
	 */
	PTE::PDEntry *pde = (PTE::PDEntry *)IdlePTD + (PD_PAGES * PT_ENTRIES - PD_PAGES);
	for (u32 i = 0; i < PD_PAGES; i++) {
		pde[i].raw = (IdlePTD + i * PAGE_SIZE) | PTE::F_S | PTE::F_W | PTE::F_P;
	}

	/* invalidate identity mapping */
	paddr_t maxMapped = roundup2(addr, PAGE_SIZE) - KERNEL_ADDRESS + LOAD_ADDRESS;
	memset((void *)IdlePTD, 0, maxMapped / (PT_ENTRIES * PAGE_SIZE) * sizeof(PTE::PDEntry));

	FlushTLB();

	quickMapPTE = VtoPTE(quickMap);
	memset(quickMapPTE, 0, QUICKMAP_SIZE * sizeof(PTE::PTEntry));
}

paddr_t
mm::VtoP(vaddr_t va)
{
	if (!VtoPDE(va)->fields.present) {
		return 0;
	}
	return (VtoPTE(va)->raw & PG_FRAME) | (va & ~PG_FRAME);
}

void *
operator new(u32 size)
{
	return mm::OpNew(size);
}

void *
operator new[](u32 size)
{
	return mm::OpNew(size);
}

void
operator delete(void *p)
{
	mm::OpDelete(p);
}

void
operator delete[](void *p)
{
	mm::OpDelete(p);
}

void *
mm::QuickMapEnter(paddr_t pa)
{
	paddr_t _pa = rounddown2(pa, PAGE_SIZE);
	for (u32 i = 0; i < QUICKMAP_SIZE; i++) {
		if (!quickMapPTE[i].fields.present) {
			quickMapPTE[i].raw = _pa | PTE::F_S | PTE::F_W | PTE::F_P;
		}
		vaddr_t va = quickMap + i * PAGE_SIZE;
		invlpg(va);
		return (void *)(va + (pa & (PAGE_SIZE - 1)));
	}
	panic("No free quick map entry");
	return 0;
}

void
mm::QuickMapRemove(vaddr_t va)
{
	va = rounddown2(va, PAGE_SIZE);
	if (va < quickMap || va >= (quickMap + QUICKMAP_SIZE * PAGE_SIZE)) {
		panic("mm::QuickMapRemove: va = 0x%x is out of quick map area", va);
	}
	PTE::PTEntry *pte = VtoPTE(va);
	if (!pte->fields.present) {
		panic("mm::QuickMapRemove: attempted to remove free quick map entry (0x%x)",
			va);
	}
	pte->raw = 0;
	invlpg(va);
}

/* usable while memory management is not initialized */
void
mm::GrowMem(vaddr_t addr)
{
	vaddr_t va = roundup2(firstAddr, PAGE_SIZE);
	vaddr_t eva = roundup2(addr, PAGE_SIZE);
	int ptGrowed = 0;
	while (va < eva) {
		PTE::PDEntry *pde = VtoPDE(va);
		if (!pde->fields.present) {
			ptGrowed = 1;
			paddr_t ptepa = (paddr_t)(eva - KERNEL_ADDRESS + LOAD_ADDRESS);
			pde->raw = ptepa | PTE::F_S | PTE::F_W | PTE::F_P;
			eva += PAGE_SIZE;
		}
		PTE::PTEntry *pte = VtoPTE(va);
		if (!pte->fields.present) {
			paddr_t pa = (paddr_t)(va - KERNEL_ADDRESS + LOAD_ADDRESS);
			pte->raw = pa | PTE::F_S | PTE::F_W | PTE::F_P;
		}
		va += PAGE_SIZE;
	}
	firstAddr = ptGrowed ? eva : addr;
}

void *
mm::malloc(u32 size, u32 align)
{
	if (!isInitialized) {
		firstAddr = roundup(firstAddr, align);
		void *p = (void *)firstAddr;
		GrowMem(firstAddr + size);
		return p;
	}

	return 0;//temp
}

void
mm::free(void *p)
{
	if (!isInitialized) {
		return;
	}

}
