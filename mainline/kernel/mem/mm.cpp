/*
 * /kernel/mem/MM.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <boot.h>

paddr_t IdlePDPT, IdlePTD;

vaddr_t quickMap;

MM *mm;

vaddr_t MM::firstAddr = 0;

/* recursive mappings */
PTE::PTEntry *MM::PTmap = (PTE::PTEntry *)PTMAP_ADDRESS;

PTE::PTEntry *MM::altPTmap = (PTE::PTEntry *)ALTPTMAP_ADDRESS;

PTE::PDEntry *MM::PTD = (PTE::PDEntry *)(PTMAP_ADDRESS +
	(PTMAP_ADDRESS >> PD_SHIFT) * PAGE_SIZE);

PTE::PDEntry *MM::altPTD = (PTE::PDEntry *)(ALTPTMAP_ADDRESS +
	(ALTPTMAP_ADDRESS >> PD_SHIFT) * PAGE_SIZE);

PTE::PDEntry *MM::PTDpde = (PTE::PDEntry *)(PTMAP_ADDRESS +
	(PTMAP_ADDRESS >> PD_SHIFT) * (PAGE_SIZE + sizeof(PTE::PTEntry)));

PTE::PDEntry *MM::altPTDpde = (PTE::PDEntry *)(ALTPTMAP_ADDRESS +
	(ALTPTMAP_ADDRESS >> PD_SHIFT) * (PAGE_SIZE + sizeof(PTE::PTEntry)));

PTE::PTEntry *MM::quickMapPTE;

MM::InitState MM::initState = IS_INITIAL;

MM::MM()
{
	assert(!mm);
	mm = this;
	kmemSlabClient = 0;
	kmemSlab = 0;
	kmemVirt = 0;
	InitAvailMem();
	InitMM();
}

const char *
MM::StrMemType(SMMemType type)
{
	switch (type) {
	case SMMT_MEMORY:
		return "RAM";
	case SMMT_RESERVED:
		return "Reserved";
	case SMMT_ACPI_RECLAIM:
		return "ACPI Reclaim";
	case SMMT_ACPI_NVS:
		return "ACPI NVS";
	case SMMT_BAD:
		return "Bad";
	}
	return "Unknown";
}

void
MM::InitAvailMem()
{
	assert(initState == IS_INITIAL);
	if (!pMBInfo || !(pMBInfo->flags & MBIF_MEMMAP)) {
		panic("No information about system memory map");
	}
	physMemSize = 0;
	int len = pMBInfo->mmapLength;
	MBIMmapEntry *pe = pMBInfo->mmapAddr;
	while (len > 0) {
		printf("[%016llx - %016llx] %s (%d)\n",
			pe->baseAddr, pe->baseAddr + pe->length,
			StrMemType((SMMemType)pe->type), pe->type);
		if (pe->type == SMMT_MEMORY) {
			physMem[physMemSize].start = pe->baseAddr;
			physMem[physMemSize].end = pe->baseAddr + pe->length;
			physMemSize++;
		}
		len -= pe->size + sizeof(pe->size);
		pe = (MBIMmapEntry *)((u8 *)pe + pe->size + sizeof(pe->size));
	}

	availMemSize = 0;
	totalMem = 0;
	for (u32 i = 0; i < physMemSize; i++) {
		for (paddr_t pa = physMem[i].start; pa < physMem[i].end; pa += PAGE_SIZE) {
			/* preserve BIOS IVT */
			if (!pa) {
				continue;
			}
			/* skip initial allocations and kernel image itself */
			if (pa >= LOAD_ADDRESS && pa < firstAddr - KERNEL_ADDRESS + LOAD_ADDRESS) {
				continue;
			}
			if (!availMemSize) {
				availMem[0].start = pa;
				availMem[0].end = pa + PAGE_SIZE;
				availMemSize++;
				totalMem++;
			} else {
				if (availMem[availMemSize - 1].end == pa) {
					availMem[availMemSize - 1].end += PAGE_SIZE;
					totalMem++;
				} else {
					if (availMemSize >= sizeof(availMem) / sizeof(availMem[0])) {
						continue;
					} else {
						availMem[availMemSize].start = pa;
						availMem[availMemSize].end = pa + PAGE_SIZE;
						availMemSize++;
						totalMem++;
					}
				}
			}
		}
	}

	initState = IS_MEMCOUNTED;
	printf("Total %dM of physical memory\n", (u32)(ptoa(totalMem) >> 20));
}

void
MM::InitMM()
{
	kmemSlabClient = new KmemSlabClient();
	assert(kmemSlabClient);
	kmemSlabInitialMem = malloc(KMEM_SLAB_INITIALMEM_SIZE);
	assert(kmemSlabInitialMem);
	kmemSlab = new SlabAllocator(kmemSlabClient, kmemSlabInitialMem,
		KMEM_SLAB_INITIALMEM_SIZE);
	assert(kmemSlab);
	kmemVirtClient = new KmemVirtClient(kmemSlab);
	assert(kmemVirtClient);
	kmemVirt = new BuddyAllocator<vaddr_t>(kmemVirtClient);
	assert(kmemVirt);
	if (CreatePageDescs()) {
		panic("MM::CreatePageDescs() failed");
	}

	initState = IS_INITIALIZING; /* malloc/mfree calls are not permitted at this level */
	kmemVirt->Initialize(firstAddr, DEV_AREA_ADDRESS - firstAddr,
		KMEM_MIN_BLOCK, KMEM_MAX_BLOCK);
	initState = IS_NORMAL;
}

void
MM::PreInitialize(vaddr_t addr)
{
	firstAddr = addr;

	/*
	 * Create page tables recursive mapping.
	 * Use identity mapping to modify PTD.
	 */
	PTE::PDEntry *pde = (PTE::PDEntry *)IdlePTD + (PTMAP_ADDRESS >> PD_SHIFT);
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
MM::VtoP(vaddr_t va)
{
	if (!VtoPDE(va)->fields.present) {
		return 0;
	}
	return (VtoPTE(va)->raw & PG_FRAME) | (va & ~PG_FRAME);
}

void *
operator new(size_t size)
{
	return MM::OpNew(size);
}

void *
operator new[](size_t size)
{
	return MM::OpNew(size);
}

void
operator delete(void *p)
{
	MM::OpDelete(p);
}

void
operator delete[](void *p)
{
	MM::OpDelete(p);
}

void *
MM::QuickMapEnter(paddr_t pa)
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
MM::QuickMapRemove(vaddr_t va)
{
	va = rounddown2(va, PAGE_SIZE);
	if (va < quickMap || va >= (quickMap + QUICKMAP_SIZE * PAGE_SIZE)) {
		panic("MM::QuickMapRemove: va = 0x%lx is out of quick map area", va);
	}
	PTE::PTEntry *pte = VtoPTE(va);
	if (!pte->fields.present) {
		panic("MM::QuickMapRemove: attempted to remove free quick map entry (0x%lx)",
			va);
	}
	pte->raw = 0;
	invlpg(va);
}

/* usable while memory management is not initialized */
void
MM::GrowMem(vaddr_t addr)
{
	if (initState > IS_MEMCOUNTED) {
		return;
	}
	vaddr_t va = roundup2(firstAddr, PAGE_SIZE);
	vaddr_t eva = roundup2(addr, PAGE_SIZE);
	while (va < eva) {
		PTE::PDEntry *pde = VtoPDE(va);
		if (!pde->fields.present) {
			paddr_t ptepa;
			if (initState == IS_INITIAL) {
				ptepa = (paddr_t)eva - KERNEL_ADDRESS + LOAD_ADDRESS;
				eva += PAGE_SIZE;
			} else {
				ptepa = _AllocPage();
			}
			pde->raw = ptepa | PTE::F_S | PTE::F_W | PTE::F_P;
		}
		PTE::PTEntry *pte = VtoPTE(va);
		if (!pte->fields.present) {
			paddr_t pa;
			if (initState == IS_INITIAL) {
				pa = (paddr_t)va - KERNEL_ADDRESS + LOAD_ADDRESS;
			} else {
				pa = _AllocPage();
			}
			pte->raw = pa | PTE::F_S | PTE::F_W | PTE::F_P;
		}
		va += PAGE_SIZE;
	}
	if (eva > firstAddr) {
		firstAddr = eva;
	}
}

paddr_t
MM::_AllocPage()
{
	if (initState == IS_MEMCOUNTED) {
		for (int i = mm->availMemSize - 1; i >= 0; i--) {
			paddr_t pa = mm->availMem[i].end - PAGE_SIZE;
			mm->availMem[i].end -= PAGE_SIZE;
			mm->totalMem--;
			if (mm->availMem[i].start >= mm->availMem[i].end) {
				mm->availMemSize--;
			}
			return pa;
		}
	}
	return 0;
}

int
MM::CreatePageDescs()
{
	/*
	 * Create page descriptors array. We don't want to waste any
	 * memory for the descriptors of the array itself so do some
	 * trick - allocate it at the top of physical memory and exclude
	 * this area from the managed pages range. Allocate additional
	 * page tables here also. Note, that array pages are mapped in reverse
	 * order - VAs growing PAs decreased.
	 */
	int chunkTop = availMemSize - 1, chunkBot = 0;
	paddr_t paTop = availMem[chunkTop].end, paBot = availMem[chunkBot].start;
	paddr_t paTopMapped = 0;
	firstAddr = roundup2(firstAddr, PAGE_SIZE);
	pages = (Page *)firstAddr;

	while (rounddown2(paTop, PAGE_SIZE) > paBot) {
		paddr_t paNext = paBot + PAGE_SIZE;
		if (paNext >= availMem[chunkBot].end) {
			chunkBot++;
			if (chunkBot >= (int)availMemSize) {
				break;
			}
			paNext = availMem[chunkBot].start;
		}
		u32 pgSize = sizeof(Page) * atop(paNext - paBot);
		do {
			if (paTop - rounddown2(paTop, PAGE_SIZE) >= pgSize) {
				paTop -= pgSize;
				pgSize = 0;
			} else {
				pgSize -= paTop - rounddown2(paTop, PAGE_SIZE);
				paTop = rounddown2(paTop, PAGE_SIZE);
				if (paTop <= availMem[chunkTop].start) {
					chunkTop--;
					paTop = availMem[chunkTop].end;
				}
				u32 sz = min(pgSize, PAGE_SIZE);
				paTop -= sz;
				pgSize -= sz;
			}
		} while (pgSize);

		paBot = paNext;
		while (rounddown2(paTop, PAGE_SIZE) != paTopMapped) {
			paddr_t pa = _AllocPage();
			paTopMapped = pa;
			PTE::PDEntry *pde = VtoPDE(firstAddr);
			if (!pde->fields.present) {
				paddr_t ptepa = _AllocPage();
				pde->raw = ptepa | PTE::F_S | PTE::F_W | PTE::F_P;
				paTopMapped = ptepa;
				pgSize = PAGE_SIZE;
				do {
					if (paTop - rounddown2(paTop, PAGE_SIZE) >= pgSize) {
						paTop -= pgSize;
						pgSize = 0;
					} else {
						pgSize -= paTop - rounddown2(paTop, PAGE_SIZE);
						paTop = rounddown2(paTop, PAGE_SIZE);
						if (paTop <= availMem[chunkTop].start) {
							chunkTop--;
							paTop = availMem[chunkTop].end;
						}
						u32 sz = min(pgSize, PAGE_SIZE);
						paTop -= sz;
						pgSize -= sz;
					}
				} while (pgSize);
			}
			PTE::PTEntry *pte = VtoPTE(firstAddr);
			pte->raw = pa | PTE::F_S | PTE::F_W | PTE::F_P;
			firstAddr += PAGE_SIZE;
		}
	}

	firstPage = atop(availMem[0].start);
	pagesRange = atop(paTop) - firstPage;
	memset(pages, 0, pagesRange * sizeof(Page));
	int curChunk = 0;
	for (u32 i = 0; i < pagesRange; i++) {
		u16 flags;
		paddr_t pa = ptoa((paddr_t)firstPage + i);
		if (pa >= availMem[curChunk].end) {
			curChunk++;
			assert((u32)curChunk < availMemSize);
		}
		if (pa < availMem[curChunk].start) {
			flags = Page::F_NOTAVAIL;
		} else {
			flags = 0;
		}
		construct(&pages[i], Page, pa, flags);
	}

	return 0;
}

MM::Page::Page(paddr_t pa, u16 flags)
{
	this->pa = pa;
	this->flags = flags;
}

void *
MM::malloc(u32 size, u32 align)
{
	if (initState <= IS_MEMCOUNTED) {
		firstAddr = roundup(firstAddr, align);
		void *p = (void *)firstAddr;
		GrowMem(firstAddr + size);
		return p;
	}
	if (initState == IS_INITIALIZING) {
		panic("MM::malloc() called on IS_INITIALIZING level");
	}
	assert(initState == IS_NORMAL);

	return 0;//temp
}

void
MM::mfree(void *p)
{
	if (initState <= IS_MEMCOUNTED) {
		return;
	}
	if (initState == IS_INITIALIZING) {
		panic("MM::mfree() called on IS_INITIALIZING level");
	}
	assert(initState == IS_NORMAL);

}

int
MM::KmemVirtClient::Allocate(vaddr_t base, vaddr_t size, void *arg)
{

	return 0;
}

int
MM::KmemVirtClient::Free(vaddr_t base, vaddr_t size, void *arg)
{

	return 0;
}
