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
	kmemMap = 0;
	kmemObj = 0;
	LIST_INIT(objects);
	numObjects = 0;
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
	kmemSlabClient = NEW(KmemSlabClient);
	assert(kmemSlabClient);
	kmemSlabInitialMem = malloc(KMEM_SLAB_INITIALMEM_SIZE);
	assert(kmemSlabInitialMem);
	kmemSlab = NEW(SlabAllocator, kmemSlabClient, kmemSlabInitialMem,
		KMEM_SLAB_INITIALMEM_SIZE);
	assert(kmemSlab);
	kmemVirtClient = NEW(KmemVirtClient, kmemSlab);
	assert(kmemVirtClient);
	if (CreatePageDescs()) {
		panic("MM::CreatePageDescs() failed");
	}
	kmemObj = NEW(VMObject, KERNEL_ADDRESS, DEV_AREA_ADDRESS - KERNEL_ADDRESS);
	assert(kmemObj);

	kmemMap = NEW(Map);
	assert(kmemMap);
	initState = IS_INITIALIZING; /* malloc/mfree calls are not permitted at this level */
	kmemMap->SetRange(firstAddr, DEV_AREA_ADDRESS - firstAddr,
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
MM::OpNew(u32 size)
{
	u32 bSize = size + sizeof(ObjOverhead);
	ObjOverhead *m;
	if (initState < IS_NORMAL) {
		m = (ObjOverhead *)malloc(bSize);
	} else {
		/* use slab allocator for kernel objects */
		m = (ObjOverhead *)mm->kmemSlab->AllocateStruct(bSize);
	}
	if (!m) {
		return 0;
	}
#ifdef DEBUG_MALLOC
	/* fill with known pattern to catch uninitialized members */
	memset(m, 0xcc, bSize);
#endif /* DEBUG_MALLOC */
	memset(m, 0, sizeof(*m));
	if (initState < IS_NORMAL) {
		m->flags = OF_NOFREE;
	}
	return m + 1;
}

void *
MM::OpNew(u32 size, const char *className, const char *fileName, int line)
{
	ObjOverhead *m = (ObjOverhead *)OpNew(size);
	if (!m) {
		return 0;
	}
	m--;
#ifdef DEBUG_MALLOC
	m->className = className;
	m->fileName = fileName;
	m->line = line;
#endif /* DEBUG_MALLOC */
	return m - 1;
}

void
MM::OpDelete(void *p)
{
	ObjOverhead *m = (ObjOverhead *)p - 1;
	if (m->flags & OF_NOFREE) {
		return;
	}
	mm->kmemSlab->FreeStruct(m);
}

void *
operator new(size_t size)
{
	return MM::OpNew(size);
}

void *
operator new[](size_t size)
{
	return MM::malloc(size);
}

void
operator delete(void *p)
{
	MM::OpDelete(p);
}

void
operator delete[](void *p)
{
	MM::mfree(p);
}

void *
operator new(size_t size, const char *className, const char *fileName, int line)
{
	return MM::OpNew(size, className, fileName, line);
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
			flags = Page::F_FREE;
		}
		construct(&pages[i], Page, pa, flags);
	}

	return 0;
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
		panic("MM::malloc() called on IS_INITIALIZING level, "
			"probably initial pool size for kernel slab allocator should be increased");
	}
	assert(initState == IS_NORMAL);
	vaddr_t m;
	if (mm->kmemMap->Allocate(size, &m)) {
		return 0;
	}
	return (void *)m;
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
	int rc = mm->kmemMap->Free((vaddr_t)p);
#ifdef DEBUG_MALLOC
	if (rc) {
		panic("MM::mfree() failed for block 0x%08lx", (vaddr_t)p);
	}
#else /* DEBUG_MALLOC */
	(void)rc;
#endif /* DEBUG_MALLOC */
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

/*************************************************************/
/* MM::Page class */
MM::Page::Page(paddr_t pa, u16 flags)
{
	this->pa = pa;
	this->flags = flags;
	object = 0;
	offset = 0;
}

/*************************************************************/
/* MM::VMObject class */

MM::VMObject::VMObject(vaddr_t base, vsize_t size)
{
	LIST_INIT(pages);
	numPages = 0;
	this->base = base;
	this->size = size;
	shadowObj = 0;
	copyObj = 0;
	refCount = 1;
}

/*************************************************************/
/* MM::Map class */
MM::Map::Map() : alloc(mm->kmemVirtClient)
{
	base = 0;
	size = 0;
	numEntries = 0;
	LIST_INIT(entries);
}

MM::Map::~Map()
{

}

int
MM::Map::SetRange(vaddr_t base, vsize_t size, int minBlockOrder, int maxBlockOrder)
{
	if (!size || this->size) {
		return -1;
	}
	int rc = alloc.Initialize(base, size, minBlockOrder, maxBlockOrder);
	if (rc) {
		return rc;
	}
	this->base = base;
	this->size = size;
	return 0;
}

int
MM::Map::AddEntry(Entry *e)
{
	entriesLock.Lock();
	LIST_ADD(list, e, entries);
	e->map = this;
	numEntries++;
	entriesLock.Unlock();
	return 0;
}

int
MM::Map::DeleteEntry(Entry *e)
{
	entriesLock.Lock();
	LIST_DELETE(list, e, entries);
	e->map = 0;
	numEntries--;
	entriesLock.Unlock();
	return 0;
}

int
MM::Map::Allocate(vsize_t size, vaddr_t *base)
{
	Entry *e = NEW(Entry, this);
	if (!e) {
		return -1;
	}
	if (alloc.Allocate(size, base, e)) {
		DELETE(e);
		return -1;
	}
	return 0;
}

int
MM::Map::Free(vaddr_t base)
{
	return alloc.Free(base);
}

/*************************************************************/
/* MM::Map::Entry class */
MM::Map::Entry::Entry(Map *map)
{
	this->map = 0;
	base = 0;
	size = 0;
	object = 0;
	offset = 0;
	flags = 0;
	map->AddEntry(this);
}

MM::Map::Entry::~Entry()
{
	map->DeleteEntry(this);
}
