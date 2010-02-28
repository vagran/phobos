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
#include <mem/SwapPager.h>

paddr_t IdlePDPT, IdlePTD;
vaddr_t vIdlePDPT, vIdlePTD; /* virtual addresses */

vaddr_t quickMap;

MM *mm;

vaddr_t MM::firstAddr = 0;

/* recursive mappings */
PTE::PTEntry *MM::PTmap = (PTE::PTEntry *)PTMAP_ADDRESS;

PTE::PTEntry *MM::altPTmap = (PTE::PTEntry *)ALTPTMAP_ADDRESS;

PTE::PDEntry *MM::PTD = (PTE::PDEntry *)(PTMAP_ADDRESS +
	PTDPTDI * PAGE_SIZE);

PTE::PDEntry *MM::altPTD = (PTE::PDEntry *)(PTMAP_ADDRESS +
	APTDPTDI * PAGE_SIZE);

PTE::PDEntry *MM::PTDpde = (PTE::PDEntry *)(PTMAP_ADDRESS +
	PTDPTDI * (PAGE_SIZE + sizeof(PTE::PTEntry)));

PTE::PDEntry *MM::altPTDpde = (PTE::PDEntry *)(PTMAP_ADDRESS +
	PTDPTDI * PAGE_SIZE + APTDPTDI * sizeof(PTE::PTEntry));

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
	devObj = 0;
	kmemAlloc = 0;
	devMemAlloc = 0;
	LIST_INIT(objects);
	LIST_INIT(topObjects);
	numObjects = 0;
	for (int i = 0; i < NUM_ZONES; i++) {
		LIST_INIT(pagesFree[i]);
		LIST_INIT(pagesCache[i]);
	}
	LIST_INIT(pagesActive);
	LIST_INIT(pagesInactive);
	LIST_INIT(maps);
	numPgFree = 0;
	numPgCache = 0;
	numPgActive = 0;
	numPgInactive = 0;
	numPgWired = 0;
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
	biosMemSize = 0;
	physMemTotal = 0;
	devPhysMem = 0;
	int len = pMBInfo->mmapLength;
	MBIMmapEntry *pe = pMBInfo->mmapAddr;
	while (len > 0) {
		biosMem[biosMemSize].location = pe->baseAddr;
		biosMem[biosMemSize].size = pe->length;
		biosMem[biosMemSize].type = (SMMemType)pe->type;
		biosMemSize++;
		/* find suitable space for memory mapped devices */
		if (!devPhysMem) {
			psize_t sz = biosMem[biosMemSize - 1].location -
				(biosMemSize > 1 ?
				biosMem[biosMemSize - 2].location + biosMem[biosMemSize - 2].size : 0);
			if (sz >= DEV_MEM_SIZE && biosMem[biosMemSize - 1].location > 16 * 1024 * 1024) {
				/* try to maximally align the address */
				devPhysMem = biosMem[biosMemSize - 1].location - DEV_MEM_SIZE;
				if (biosMemSize > 1) {
					int order = 1;
					paddr_t minAddr = biosMem[biosMemSize - 2].location + biosMem[biosMemSize - 2].size;
					while (order < 32) {
						paddr_t addr = devPhysMem & ~((1 << order) - 1);
						if (addr < minAddr) {
							break;
						}
						devPhysMem = addr;
						order++;
					}
				} else {
					devPhysMem = 0;
				}
				klog(KLOG_INFO, "Devices memory at 0x%016llx - 0x%016llx",
					devPhysMem, devPhysMem + DEV_MEM_SIZE);
			}
		}
		if (pe->type == SMMT_MEMORY) {
			physMem[physMemSize].start = pe->baseAddr;
			physMem[physMemSize].end = pe->baseAddr + pe->length;
			physMemSize++;
			physMemTotal += pe->length;
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
}

int
MM::PrintMemInfo(ConsoleDev *dev)
{
	for (u32 i = 0; i < biosMemSize; i++) {
		BiosMemChunk *bmc = &biosMem[i];
		printf("[%016llx - %016llx] %s (%d)\n",
					bmc->location, bmc->location + bmc->size,
					StrMemType((SMMemType)bmc->type), bmc->type);
	}
	dev->Printf("Total %luM of physical memory\n", (u32)(physMemTotal >> 20));
	return 0;
}

void
MM::InitMM()
{
	kmemSlabClient = NEWSINGLE(KmemSlabClient);
	assert(kmemSlabClient);
	kmemSlabInitialMem = malloc(KMEM_SLAB_INITIALMEM_SIZE);
	assert(kmemSlabInitialMem);
	kmemSlab = NEW(SlabAllocator, kmemSlabClient, kmemSlabInitialMem,
		KMEM_SLAB_INITIALMEM_SIZE);
	assert(kmemSlab);
	kmemMapClient = NEWSINGLE(KmemMapClient, kmemSlab);
	assert(kmemMapClient);
	devMemClient = NEWSINGLE(PABuddyClient, kmemSlab);
	assert(devMemClient);
	devMemAlloc = NEWSINGLE(BuddyAllocator<paddr_t>, devMemClient);
	assert(devMemAlloc);
	if (devMemAlloc->Initialize(devPhysMem, DEV_MEM_SIZE, PAGE_SHIFT, 30)) {
		panic("Devices physical memory allocator initialization failed");
	}
	if (CreatePageDescs()) {
		panic("MM::CreatePageDescs() failed");
	}
	kmemObj = NEW(VMObject, KERN_DYN_MEM_END - KERNEL_ADDRESS);
	assert(kmemObj);
	devObj = NEW(VMObject, DEV_MEM_SIZE);
	assert(devObj);

	kmemMap = NEW(Map, (PTE::PDEntry *)vIdlePDPT, (PTE::PDEntry *)vIdlePTD, 1);
	assert(kmemMap);
	initState = IS_INITIALIZING; /* malloc/mfree calls are not permitted at this level */
	kmemMap->SetRange(0, KERN_MEM_END,
		KMEM_MIN_BLOCK, KMEM_MAX_BLOCK);
	LIST_ADD(list, kmemMap, maps);
	kmemObj->SetSize(KERN_DYN_MEM_END - firstAddr);

	kmemEntry = kmemMap->InsertObjectAt(kmemObj, firstAddr);
	if (!kmemEntry) {
		panic("Kernel dynamic memory object insertion failed");
	}
	devEntry = kmemMap->InsertObjectAt(devObj, DEV_MEM_ADDRESS);
	if (!devEntry) {
		panic("Kernel devices memory object insertion failed");
	}
	devEntry->flags |= Map::Entry::F_SPACE | Map::Entry::F_NOCACHE;

	/* release initial reference */
	kmemObj->Release();
	devObj->Release();
	kmemAlloc = kmemEntry->CreateAllocator();
	assert(kmemAlloc);
	devAlloc = devEntry->CreateAllocator();
	assert(devAlloc);
	initState = IS_NORMAL;

	idt->RegisterHandler(IDT::ST_PAGEFAULT, OnPageFault, this);
}

paddr_t
MM::AllocDevPhys(psize_t size)
{
	paddr_t addr;
	size = roundup2(size, PAGE_SIZE);
	if (devMemAlloc->Allocate(size, &addr)) {
		klog(KLOG_WARNING,
			"Failed to allocate %llu bytes of devices physical memory space",
			size);
		return 0;
	}
	return addr;
}

int
MM::FreeDevPhys(paddr_t addr)
{
	return devMemAlloc->Free(addr);
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
	FlushTLB();

	/* invalidate identity mapping */
	paddr_t maxMapped = roundup2(addr, PAGE_SIZE * PT_ENTRIES) - KERNEL_ADDRESS + LOAD_ADDRESS;
	memset((void *)PTD, 0, maxMapped / (PT_ENTRIES * PAGE_SIZE) * sizeof(PTE::PDEntry));
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

paddr_t
MM::Kextract(vaddr_t va)
{
	if (kmemMap) {
		return kmemMap->Extract(va);
	}
	return VtoP(va);
}

vaddr_t
MM::MapDevPhys(paddr_t pa, psize_t size)
{
	size = roundup2(size, PAGE_SIZE);
	vaddr_t sva = (vaddr_t)devAlloc->malloc(size);
	if (!sva) {
		return 0;
	}
	vaddr_t va = sva;
	while (size) {
		devEntry->MapPA(va, pa);
		va += PAGE_SIZE;
		pa += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	return sva;
}

void *
MM::OpNew(u32 size, int isSingle)
{
	u32 bSize = size + sizeof(ObjOverhead);
	ObjOverhead *m;
	if (initState < IS_INITIALIZING) {
		m = (ObjOverhead *)malloc(bSize);
	} else {
		/* use slab allocator for kernel objects */
		if (isSingle) {
			m = (ObjOverhead *)mm->kmemSlab->malloc(bSize);
		} else {
			m = (ObjOverhead *)mm->kmemSlab->AllocateStruct(bSize);
		}
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
MM::OpNew(u32 size, int isSingle, const char *className, const char *fileName, int line)
{
	ObjOverhead *m = (ObjOverhead *)OpNew(size, isSingle);
	if (!m) {
		return 0;
	}
	m--;
#ifdef DEBUG_MALLOC
	m->className = className;
	m->fileName = fileName;
	m->line = line;
#endif /* DEBUG_MALLOC */
	return m + 1;
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
operator new(size_t size, int isSingle)
{
	return MM::OpNew(size, isSingle);
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
operator new(size_t size, int isSingle, const char *className, const char *fileName, int line)
{
	return MM::OpNew(size, isSingle, className, fileName, line);
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

void
MM::ZeroPage(paddr_t pa)
{
	assert(!(pa & ~PG_FRAME));
	void *p = QuickMapEnter(pa);
	memset(p, 0, PAGE_SIZE);
	QuickMapRemove((vaddr_t)p);
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
			ZeroPage(ptepa);
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
				ZeroPage(ptepa);
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
		if (flags & Page::F_FREE) {
			PageZone zone = pages[i].GetZone();
			numPgFree++;
			LIST_ADD(queue, &pages[i], pagesFree[zone]);
		}
	}

	return 0;
}

MM::Map *
MM::CreateMap()
{
	Map *m = NEW(Map, kmemMap);
	if (!m) {
		klog(KLOG_WARNING, "Cannot create map (no memory)");
		return 0;
	}
	mapsLock.Lock();
	LIST_ADD(list, m, maps);
	mapsLock.Unlock();
	ensure(!m->SetRange(0, ALTPTMAP_ADDRESS));
	return m;
}

int
MM::UpdatePDE(Map *originator, vaddr_t va, PTE::PDEntry *pde)
{
	if (va < KERNEL_ADDRESS) {
		return 0;
	}
	u32 x = IM::DisableIntr();
	mapsLock.Lock();
	Map *m;
	LIST_FOREACH(Map, list, m, maps) {
		if (m == originator) {
			continue;
		}
		m->AddPDE(va, pde);
	}
	mapsLock.Unlock();
	IM::RestoreIntr(x);
	return 0;
}

MM::Page *
MM::GetPage(paddr_t pa)
{
	u32 pgIdx = atop(pa);
	if (pgIdx < firstPage || pgIdx >= firstPage + pagesRange) {
		return 0;
	}
	return &pages[pgIdx];
}

MM::Page *
MM::GetFreePage(int zone)
{
	if (zone >= NUM_ZONES) {
		return 0;
	}
	Page *p = 0;
	while (zone >= 0) {
		if (LIST_ISEMPTY(pagesFree[zone])) {
			zone--;
			continue;
		}
		p = LIST_FIRST(Page, queue, pagesFree[zone]);
		break;
	}
	return p;
}

MM::Page *
MM::GetCachedPage(PageZone zone)
{
	Page *p = 0;
	while (zone < NUM_ZONES) {
		if (LIST_ISEMPTY(pagesCache[zone])) {
			continue;
		}
		p = LIST_FIRST(Page, queue, pagesCache[zone]);
		break;
	}
	return p;
}

MM::Page *
MM::AllocatePage(int flags, PageZone zone)
{
	/* XXX need management implementation */
	mm->pgqLock.Lock();
	Page *p = GetFreePage(zone);
	if (!p) {
		mm->pgqLock.Unlock();
		return 0;
	}
	p->Unqueue();
	mm->pgqLock.Unlock();
	p->flags = (p->flags & ~Page::F_ZONEMASK) | zone;
	return p;
}

int
MM::FreePage(Page *pg)
{
	/* notimpl */
	return 0;
}

int
MM::MapPhys(vaddr_t va, paddr_t pa)
{
	return kmemEntry->MapPA(va, pa);
}

int
MM::UnmapPhys(vaddr_t va)
{
	return kmemEntry->Unmap(va);
}

void *
MM::malloc(u32 size, u32 align, PageZone zone)
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
	/* align is ignored */
	assert(initState == IS_NORMAL);
	return mm->kmemAlloc->malloc(size, (void *)zone);
}

void
MM::mfree(void *p)
{
	assert(p);
	if (initState <= IS_MEMCOUNTED) {
		return;
	}
	if (initState == IS_INITIALIZING) {
		panic("MM::mfree() called on IS_INITIALIZING level");
	}
	assert(initState == IS_NORMAL);
	mm->kmemAlloc->mfree(p);
}

int
MM::KmemMapClient::Allocate(vaddr_t base, vaddr_t size, void *arg)
{
	Map::Entry *e = (Map::Entry *)arg;
	e->base = base;
	e->size = size;
	return 0;
}

int
MM::KmemMapClient::Free(vaddr_t base, vaddr_t size, void *arg)
{
	DELETE((Map::Entry *)arg);
	return 0;
}

int
MM::KmemMapClient::Reserve(vaddr_t base, vaddr_t size, void *arg)
{
	Map::Entry *e = (Map::Entry *)arg;
	e->base = base;
	e->size = size;
	e->flags |= Map::Entry::F_RESERVE;
	return 0;
}

int
MM::KmemMapClient::UnReserve(vaddr_t base, vaddr_t size, void *arg)
{
	DELETE((Map::Entry *)arg);
	return 0;
}

int
MM::OnPageFault(Frame *frame, void *arg)
{
	vaddr_t va = rcr2();
	int isUserMode = GDT::GetRPL(frame->cs) == GDT::PL_USER;
	if (((MM *)arg)->OnPageFault(va, frame->code, isUserMode)) {
		if (isUserMode) {
			/* send signal to process */
			//notimpl
		} else {
			printf("Unhandled page fault in kernel mode: "
				"at 0x%08lx accessing address 0x%08lx, code = 0x%08lx\n",
				frame->eip, va, frame->code);
			if (Debugger::debugFaults && sysDebugger) {
				sysDebugger->Trap(frame);
			}
			panic("Unhandled page fault in kernel mode");
		}
	}
	return 0;
}

int
MM::OnPageFault(vaddr_t va, u32 code, int isUserMode)
{
	printf("Processing page fault on 0x%08lx, code = 0x%08lx, userMode = %d\n",
		va, code, isUserMode);//temp
	//notimpl
	return -1;
}

/*************************************************************/
/* MM::Page class */

MM::Page::Page(paddr_t pa, u16 flags)
{
	this->pa = pa;
	this->flags = flags;
	object = 0;
	wireCount = 0;
}

MM::PageZone
MM::Page::GetZone()
{
	if (pa >= 0x100000000ull) {
		return ZONE_REST;
	}
	if (pa >= 0x1000000ull) {
		return ZONE_4GB;
	}
	if (pa >= 0x100000ull) {
		return ZONE_16MB;
	}
	return ZONE_1MB;
}

int
MM::Page::Unqueue()
{
	int rc = 0;
	PageZone zone = GetZone();
	if (flags & F_FREE) {
		mm->numPgFree--;
		LIST_DELETE(queue, this, mm->pagesFree[zone]);
		flags &= ~F_FREE;
	} else if (flags & F_CACHE) {
		mm->numPgCache--;
		LIST_DELETE(queue, this, mm->pagesCache[zone]);
		flags &= ~F_CACHE;
	} else if (flags & F_ACTIVE) {
		mm->numPgActive--;
		LIST_DELETE(queue, this, mm->pagesActive);
		flags &= ~F_ACTIVE;
	} else if (flags & F_INACTIVE) {
		mm->numPgInactive--;
		LIST_DELETE(queue, this, mm->pagesInactive);
		flags &= ~F_INACTIVE;
	} else {
		rc = 1;
	}
	return rc;
}

int
MM::Page::Activate()
{
	assert(!wireCount);
	assert(!(flags & F_FREE));
	if (wireCount || (flags & F_FREE)) {
		return -1;
	}
	if (flags & F_ACTIVE) {
		return 0;
	}
	mm->pgqLock.Lock();
	if (flags & (F_INACTIVE | F_CACHE)) {
		Unqueue();
	}
	mm->numPgActive++;
	LIST_ADD(queue, this, mm->pagesActive);
	mm->pgqLock.Unlock();
	return 0;
}

int
MM::Page::Wire()
{
	if (flags & (F_FREE | F_ACTIVE | F_INACTIVE | F_CACHE)) {
		return -1;
	}
	AtomicOp::Inc(&wireCount);
	return 0;
}

/*************************************************************/
/* MM::Pager class */

MM::Pager *
MM::Pager::CreatePager(Type type, vsize_t size, Handle handle)
{
	Pager *p;
	switch (type) {
	case T_SWAP:
		p = NEW(SwapPager, size, handle);
		break;
	default:
		panic("Invalid pager type: %d", type);
	}
	if (!p) {
		klog(KLOG_WARNING, "Failed to allocate memory for pager");
	}
	return p;
}

MM::Pager::Pager(Type type, vsize_t size, Handle handle)
{
	this->type = type;
	assert(size);
	this->size = size;
	this->handle = handle;
}

MM::Pager::~Pager()
{

}

/*************************************************************/
/* MM::VMObject class */

MM::VMObject::VMObject(vsize_t size, u32 flags)
{
	TREE_INIT(pages);
	numPages = 0;
	pager = 0;
	this->size = size;
	copyObj = 0;
	copyOffset = 0;
	LIST_INIT(shadowObj);
	this->flags = flags;
}

MM::VMObject::~VMObject()
{
	assert(!refCount);
	if (pager) {
		pager->Release();
	}
}

int
MM::VMObject::SetSize(vsize_t size)
{
	this->size = size;
	/* XXX should drop all resident pages */
	/* truncate shadows */
	VMObject *obj;
	LIST_FOREACH(VMObject, shadowList, obj, shadowObj) {
		vsize_t newSize = size - obj->copyOffset;
		if (obj->size > newSize) {
			obj->SetSize(newSize);
		}
	}
	return 0;
}

int
MM::VMObject::InsertPage(Page *pg, vaddr_t offset)
{
	int rc;
	assert(!LookupPage(offset));
	lock.Lock();
	if (flags & F_NOTPAGEABLE) {
		rc = pg->Wire();
	} else {
		rc = pg->Activate();
	}
	if (rc)	 {
		lock.Unlock();
		return rc;
	}
	pg->object = this;
	pg->offset = offset;
	TREE_ADD(objEntry, pg, pages, offset);
	numPages++;
	lock.Unlock();
	return 0;
}

MM::Page *
MM::VMObject::LookupPage(vaddr_t offset)
{
	lock.Lock();
	Page *pg = TREE_FIND(rounddown2(offset, PAGE_SIZE), Page, objEntry, pages);
	lock.Unlock();
	return pg;
}

int
MM::VMObject::Pagein(vaddr_t offset, Page **ppg)
{
	offset = rounddown2(offset, PAGE_SIZE);
	if (offset >= size) {
		panic("Offset out of range: 0x%08lx", offset);
	}
	if (!pager) {
		pager = Pager::CreatePager(Pager::T_DEFAULT, size);
		if (!pager) {
			klog(KLOG_ERROR, "Cannot create pager");
			return -1;
		}
	}
	Page *pg = mm->AllocatePage();
	if (!pg) {
		klog(KLOG_WARNING, "Cannot allocate page");
		return -1;
	}
	if (pager->GetPage(pg, offset)) {
		mm->FreePage(pg);
		klog(KLOG_ERROR, "Cannot get page from pager");
		return -1;
	}
	ensure(!InsertPage(pg, offset));
	if (ppg) {
		*ppg = pg;
	}
	return 0;
}

int
MM::VMObject::Pageout(vaddr_t offset, Page **ppg)
{
	offset = rounddown2(offset, PAGE_SIZE);
	if (offset >= size) {
		panic("Offset out of range: 0x%08lx", offset);
	}
	if (!pager) {
		pager = Pager::CreatePager(Pager::T_DEFAULT, size);
		if (!pager) {
			klog(KLOG_ERROR, "Cannot create pager");
			return -1;
		}
	}

	return 0;
}

/*************************************************************/
/* MM::Map class */

MM::Map::Map(Map *copyFrom) : alloc(mm->kmemMapClient), mapEntryClient(mm->kmemSlab)
{
	int rc = Initialize();
	assert(!rc);
	(void)rc;
	freeTables = 1;
	ptd = (PTE::PDEntry *)malloc(PD_PAGES * PAGE_SIZE);
	assert(ptd);
	assert(!((u32)ptd & (PAGE_SIZE - 1)));
	memset(ptd, 0, PD_PAGES * PAGE_SIZE);
	if (copyFrom) {
		u32 startIdx = KERNEL_ADDRESS >> PD_SHIFT;
		copyFrom->tablesLock.Lock();
		memcpy(&ptd[startIdx], &copyFrom->ptd[startIdx],
			PD_PAGES * PAGE_SIZE - startIdx * sizeof(PTE::PDEntry));
		for (u32 i = startIdx; i < PT_ENTRIES * PD_PAGES; i++) {
			if (i >= PTDPTDI && i < PTDPTDI + PD_PAGES) {
				continue;
			}
			PTE::PDEntry *pde = &ptd[i];
			if (pde->fields.present) {
				paddr_t ptpa = pde->raw & PG_FRAME;
				Page *pg = mm->GetPage(ptpa);
				if (pg) {
					pg->Wire();
				}
			}
		}
		copyFrom->tablesLock.Unlock();
	}
	pdpt = (PTE::PDEntry *)malloc(sizeof(PTE::PDEntry) * PD_PAGES, 0, ZONE_4GB);
	assert(pdpt);
	/* should be properly aligned */
	assert(!((u32)pdpt & (sizeof(PTE::PDEntry) * PD_PAGES - 1)));
	memset(pdpt, 0, sizeof(PTE::PDEntry) * PD_PAGES);
	assert(mm->Kextract((vaddr_t)pdpt) <= 0xffffffffull);
	cr3 = (u32)mm->Kextract((vaddr_t)pdpt);
	for (int i = 0; i < PD_PAGES; i++) {
		paddr_t pa = mm->Kextract((vaddr_t)ptd + i * PAGE_SIZE);
		pdpt[i].raw = pa | PTE::F_P;
		/* recursive entries */
		ptd[PTDPTDI + i].raw = pa | PTE::F_S | PTE::F_W | PTE::F_P;
	}
}

MM::Map::Map(PTE::PDEntry *pdpt, PTE::PDEntry *ptd, int noFree) :
	alloc(mm->kmemMapClient), mapEntryClient(mm->kmemSlab)
{
	int rc = Initialize();
	assert(!rc);
	(void)rc;
	freeTables = !noFree;
	this->pdpt = pdpt;
	this->ptd = ptd;
	assert(mm->Kextract((vaddr_t)pdpt) <= 0xffffffffull);
	cr3 = (u32)mm->Kextract((vaddr_t)pdpt);
}

MM::Map::~Map()
{
	FreeSubmaps(&submaps);
	if (freeTables) {
		mfree(pdpt);
		mfree(ptd);
	}
}

int
MM::Map::Initialize()
{
	base = 0;
	size = 0;
	numEntries = 0;
	LIST_INIT(entries);
	LIST_INIT(submaps);
	parentMap = 0;
	rootMap = this;
	return 0;
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

MM::Map *
MM::Map::CreateSubmap(vaddr_t base, vsize_t size)
{
	if ((base & (PAGE_SIZE - 1)) || (size & (PAGE_SIZE - 1))) {
		panic("Region is not page aligned");
	}
	if (base < this->base || base + size > this->base + this->size) {
		klog(KLOG_ERROR,
			"Attempted to allocate submap out of map range 0x%08lx - 0x%08lx/0x%08lx - 0x%08lx",
			base, base + size, this->base, this->base + this->size);
		return 0;
	}
	Entry *e = ReserveSpace(base, size);
	if (!e) {
		klog(KLOG_DEBUG, "No space for submap");
		return 0;
	}
	e->flags |= Entry::F_SUBMAP;
	Map *submap = NEW(Map, rootMap->pdpt, rootMap->ptd, 1);
	if (!submap) {
		klog(KLOG_WARNING, "Failed to allocate submap (no memory)");
		UnReserveSpace(base);
		return 0;
	}
	submap->parentMap = this;
	submap->rootMap = this->rootMap;
	u16 minOrder, maxOrder;
	alloc.GetOrders(&minOrder, &maxOrder);
	ensure(!submap->SetRange(base, size, minOrder, maxOrder));
	smListLock.Lock();
	LIST_ADD(smList, submap, submaps);
	smListLock.Unlock();
	return submap;
}

int
MM::Map::FreeSubmaps(ListHead *submaps)
{
	if (!submaps) {
		return 0;
	}
	Map *m;
	smListLock.Lock();
	while ((m = LIST_FIRST(Map, smList, *submaps))) {
		LIST_DELETE(smList, m, *submaps);
		DELETE(m);
	}
	smListLock.Unlock();
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

MM::Map::Entry *
MM::Map::Allocate(vsize_t size, vaddr_t *base, int fixed)
{
	Entry *e = NEW(Entry, this);
	if (!e) {
		return 0;
	}
	int rc;
	if (fixed) {
		rc = alloc.AllocateFixed(*base, size, e);
	} else {
		rc = alloc.Allocate(size, base, e);
	}
	if (rc) {
		DELETE(e);
		return 0;
	}
	return e;
}

MM::Map::Entry *
MM::Map::AllocateSpace(vsize_t size, vaddr_t *base, int fixed)
{
	Entry *e = NEW(Entry, this);
	if (!e) {
		return 0;
	}
	e->flags |= Entry::F_SPACE;
	int rc;
	if (fixed) {
		rc = alloc.AllocateFixed(*base, size, e);
	} else {
		rc = alloc.Allocate(size, base, e);
	}
	if (rc) {
		DELETE(e);
		return 0;
	}
	return e;
}

MM::Map::Entry *
MM::Map::ReserveSpace(vaddr_t base, vsize_t size)
{
	Entry *e = NEW(Entry, this);
	if (!e) {
		return 0;
	}
	e->flags |= Entry::F_RESERVE;
	if (alloc.Reserve(base, size, e)) {
		DELETE(e);
		return 0;
	}
	return e;
}

MM::Map::Entry *
MM::Map::Lookup(vaddr_t base)
{
	Entry *e;
	if (alloc.Lookup(base, 0, 0, (void **)&e,
		BuddyAllocator<vaddr_t>::LUF_ALLOCATED | BuddyAllocator<vaddr_t>::LUF_RESERVED)) {
		return 0;
	}
	return e;
}

MM::Map::Entry *
MM::Map::IntInsertObject(VMObject *obj, vaddr_t base, int fixed,
			vaddr_t offset, vsize_t size)
{
	if (offset & (PAGE_SIZE - 1)) {
		return 0;
	}
	if (size != VSIZE_MAX && (size & (PAGE_SIZE - 1))) {
		return 0;
	}
	if (offset >= obj->size || !size) {
		return 0;
	}
	if (size != VSIZE_MAX) {
		if (offset + size > obj->size) {
			return 0;
		}
	} else {
		size = obj->size - offset;
	}
	Entry *e = Allocate(size, &base, fixed);
	if (!e) {
		return 0;
	}
	obj->AddRef();
	e->object = obj;
	e->offset = offset;
	return e;
}

MM::Map::Entry *
MM::Map::InsertObject(VMObject *obj, vaddr_t offset, vsize_t size)
{
	return IntInsertObject(obj, 0, 0, offset, size);
}

MM::Map::Entry *
MM::Map::InsertObjectAt(VMObject *obj, vaddr_t base, vaddr_t offset, vsize_t size)
{
	return IntInsertObject(obj, base, 1, offset, size);
}

int
MM::Map::IsCurrent()
{
	/* compare this map PTD addresses with currently mapped */
	for (int i = 0; i < PD_PAGES; i++) {
		if ((ptd[PTDPTDI + i].raw ^ PTDpde[i].raw) & PG_FRAME) {
			return 0;
		}
	}
	return 1;
}

int
MM::Map::IsAlt()
{
	/* compare this map PTD addresses with currently mapped in alternative AS */
	for (int i = 0; i < PD_PAGES; i++) {
		if ((ptd[PTDPTDI + i].raw ^ altPTDpde[i].raw) & PG_FRAME) {
			return 0;
		}
	}
	return 1;
}

void
MM::Map::SetAlt()
{
	for (int i = 0; i < PD_PAGES; i++) {
		altPTDpde[i].raw = ptd[PTDPTDI + i].raw;
	}
	FlushTLB();
}

PTE::PDEntry *
MM::Map::GetPDE(vaddr_t va)
{
	return &ptd[va >> PD_SHIFT];
}

PTE::PTEntry *
MM::Map::GetPTE(vaddr_t va)
{
	/* first check if we have valid PDE */
	PTE::PDEntry *pde = GetPDE(va);
	if (!pde->fields.present) {
		return 0;
	}
	/* second step is ensure we can access PTE by VA */
	PTE::PTEntry *pte;
	if (IsCurrent()) {
		pte = VtoPTE(va);
	} else {
		if (!IsAlt()) {
			SetAlt();
		}
		pte = VtoAPTE(va);
	}
	return pte;
}

paddr_t
MM::Map::Extract(vaddr_t va)
{
	PTE::PTEntry *pte = GetPTE(va);
	if (!pte) {
		return 0;
	}
	if (!pte->fields.present) {
		return 0;
	}
	return (pte->raw & PG_FRAME) | (va & ~PG_FRAME);
}

int
MM::Map::IsMapped(vaddr_t va)
{
	PTE::PTEntry *pte = GetPTE(va);
	if (!pte) {
		return 0;
	}
	return pte->fields.present;
}

int
MM::Map::Free(vaddr_t base)
{
	return alloc.Free(base);
}

int
MM::Map::UnReserveSpace(vaddr_t base)
{
	return alloc.UnReserve(base);
}

int
MM::Map::AddPT(vaddr_t va)
{
	PTE::PDEntry *pde = GetPDE(va);
	if (pde->fields.present) {
		return 0;
	}
	Page *pg = mm->AllocatePage();
	if (!pg) {
		return -1;
	}
	pg->Wire();
	ZeroPage(pg->pa);
	pde->raw = pg->pa | PTE::F_P | PTE::F_S | PTE::F_W;
	mm->UpdatePDE(this, va, pde);
	return 0;
}

int
MM::Map::AddPDE(vaddr_t va, PTE::PDEntry *pde)
{
	PTE::PDEntry *locPde = GetPDE(va);
	assert(!locPde->fields.present);
	locPde->raw = pde->raw;
	paddr_t ptpa = pde->raw & PG_FRAME;
	Page *pg = mm->GetPage(ptpa);
	assert(pg);
	pg->Wire();
	return 0;
}

int
MM::Map::Pagein(vaddr_t va)
{
	Entry *e = Lookup(va);
	if (!e) {
		return -1;
	}
	/* do not process space reservation entries */
	if (e->flags & (Entry::F_SPACE | Entry::F_RESERVE)) {
		return -1;
	}
	ensure(e->object);
	Page *pg;
	vaddr_t offset = 0;
	ensure(!e->GetOffset(va, &offset));
	if (e->object->Pagein(offset, &pg)) {
		return -1;
	}
	ensure(!e->MapPage(rounddown2(va, PAGE_SIZE), pg));
	return 0;
}

/*************************************************************/
/* MM::Map::MapEntryClient class */

/* allocate memory chunk in the map entry, set it resident */
int
MM::Map::MapEntryClient::Allocate(vaddr_t base, vaddr_t size, void *arg)
{
	Entry::EntryAllocParam *eap = (Entry::EntryAllocParam *)arg;
	Entry *e = eap->e;
	VMObject *obj = e->object;
	assert(base + size <= e->base + e->size);
	if ((e->flags & Entry::F_SPACE) || (e->flags & Entry::F_RESERVE)) {
		return 0;
	}

	for (vaddr_t sva = rounddown2(base, PAGE_SIZE),
		eva = roundup2(base + size, PAGE_SIZE);
		sva < eva;
		sva += PAGE_SIZE) {

		vaddr_t offs;
		if (e->GetOffset(sva, &offs)) {
			panic("Failed to get offset");
		}
		Page *pg = obj->LookupPage(offs);
		if (pg) {
			continue;
		}
		pg = mm->AllocatePage(0, eap->zone);
		if (!pg) {
			/* XXX should free all pages allocated so far */
			return -1;
		}
		obj->InsertPage(pg, offs);
		e->MapPage(sva, pg);
	}
	return 0;
}

int
MM::Map::MapEntryClient::Free(vaddr_t base, vaddr_t size, void *arg)
{
	/* XXX */
	return 0;
}

int
MM::Map::MapEntryClient::Reserve(vaddr_t base, vaddr_t size, void *arg)
{
	return 0;
}

int
MM::Map::MapEntryClient::UnReserve(vaddr_t base, vaddr_t size, void *arg)
{
	return 0;
}

/*************************************************************/
/* MapEntryAllocator class */

MM::Map::MapEntryAllocator::MapEntryAllocator(Entry *e) :
	alloc(&e->map->mapEntryClient)
{
	alloc.Initialize(e->base, e->size, MIN_BLOCK, MAX_BLOCK);
	this->e = e;
	assert(!e->alloc);
	e->alloc = this;
}

MM::Map::MapEntryAllocator::~MapEntryAllocator()
{

}

void *
MM::Map::MapEntryAllocator::malloc(u32 size, void *param)
{
	vaddr_t addr;
	Entry::EntryAllocParam eap;

	eap.e = e;
	eap.zone = (PageZone)(u32)param;
	if (alloc.Allocate(size, &addr, &eap)) {
		return 0;
	}
	return (void *)addr;
}

void
MM::Map::MapEntryAllocator::mfree(void *p)
{
	alloc.Free((vaddr_t)p);
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
	alloc = 0;
	map->AddEntry(this);
}

MM::Map::Entry::~Entry()
{
	if (alloc)	{
		alloc->Release();
	}
	map->DeleteEntry(this);
}

MemAllocator *
MM::Map::Entry::CreateAllocator()
{
	return NEW(MapEntryAllocator, this);
}

int
MM::Map::Entry::MapPA(vaddr_t va, paddr_t pa)
{
	map->tablesLock.Lock();
	PTE::PDEntry *pde = map->GetPDE(va);
	if (!pde->fields.present) {
		if (map->AddPT(va)) {
			map->tablesLock.Unlock();
			return -1;
		}
	}
	PTE::PTEntry *pte = map->GetPTE(va);
	/* XXX should use entry protection */
	pte->raw = pa | PTE::F_P | PTE::F_S | PTE::F_W |
		((flags & F_NOCACHE) ? PTE::F_WT | PTE::F_CD : 0);
	invlpg(va);
	map->tablesLock.Unlock();
	return 0;
}

int
MM::Map::Entry::Unmap(vaddr_t va)
{
	map->tablesLock.Lock();
	PTE::PDEntry *pde = map->GetPDE(va);
	if (!pde->fields.present) {
		map->tablesLock.Unlock();
		return -1;
	}
	PTE::PTEntry *pte = map->GetPTE(va);
	pte->raw = 0;
	map->tablesLock.Unlock();
	return 0;
}

int
MM::Map::Entry::GetOffset(vaddr_t va, vaddr_t *offs)
{
	if (va < base || va >= base + size) {
		return -1;
	}
	if (offs) {
		*offs = offset + va - base;
	}
	return 0;
}

int
MM::Map::Entry::MapPage(vaddr_t va, Page *pg)
{
	assert(!(va & ~PG_FRAME));
	if (!pg) {
		vaddr_t offs;
		if (GetOffset(va, &offs)) {
			return -1;
		}
		pg = object->LookupPage(offs);
		if (!pg) {
			return -1;
		}
	}
	return MapPA(va, pg->pa);
}
