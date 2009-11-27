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

#include <boot.h>
#include <SlabAllocator.h>
#include <BuddyAllocator.h>

class MM {
public:
	enum {
		QUICKMAP_SIZE =		8, /* pages */
		MEM_MAX_CHUNKS =	16,
	};

	typedef struct {
		paddr_t	start, end;
	} MemChunk;

	MemChunk physMem[MEM_MAX_CHUNKS];
	u32 physMemSize;
	MemChunk availMem[MEM_MAX_CHUNKS];
	u32 availMemSize;
	u32 totalMem;
	SlabAllocator *kmemSlab;
	BuddyAllocator<vaddr_t> *kmemVirt; /* kernel virtual address space allocator */

	typedef enum {
		PG_FREE =		0x1,
		PG_ACITIVE =	0x2,
		PG_INACTIVE =	0x4,
	} PageFlags;

	typedef struct {
		paddr_t		pa;
		u16			flags;

	} Page;
private:
	typedef enum {
		IS_INITIAL,
		IS_MEMCOUNTED,
		IS_NORMAL,
	} InitState;

	enum {
		KMEM_SLAB_INITIALMEM_SIZE = 128 * 1024,
	};

	static vaddr_t firstAddr;
	static PTE::PTEntry *PTmap;
	static PTE::PDEntry *PTD, *PTDpde;
	static PTE::PTEntry *quickMapPTE;
	static InitState initState;

	class KmemSlabClient : public SlabAllocator::SlabClient {
	public:
		virtual void *malloc(u32 size) { return MM::malloc(size); }
		virtual void mfree(void *p) { return MM::mfree(p); }
		virtual void FreeInitialPool(void *p, u32 size) {}
	};

	class KmemVirtClient : public BuddyAllocator<vaddr_t>::BuddyClient {
	private:
		MemAllocator *m;
	public:
		KmemVirtClient(MemAllocator *m) { this->m = m; }
		virtual void *malloc(u32 size) { return m->malloc(size); }
		virtual void mfree(void *p) { return m->mfree(p); }
		virtual int Allocate(vaddr_t base, vaddr_t size, void *arg = 0);
		virtual int Free(vaddr_t base, vaddr_t size, void *arg = 0);
	};

	KmemSlabClient *kmemSlabClient;
	void *kmemSlabInitialMem;
	KmemVirtClient *kmemVirtClient;

	static inline void FlushTLB() {wcr3(rcr3());}
	static void GrowMem(vaddr_t addr);
	static paddr_t _AllocPage(); /* at IS_MEMCOUNTED level */

	void InitAvailMem();
	void InitMM();
	const char *StrMemType(SMMemType type);

public:
	static void PreInitialize(vaddr_t addr);
	static paddr_t VtoP(vaddr_t va);
	static inline PTE::PDEntry *VtoPDE(vaddr_t va) {return &PTD[va >> PD_SHIFT];}
	static inline PTE::PTEntry *VtoPTE(vaddr_t va) {return &PTmap[va >> PT_SHIFT];}
	static inline void *OpNew(u32 size) {return malloc(size);}
	static inline void OpDelete(void *p) {return mfree(p);}
	static void *malloc(u32 size, u32 align = 4);
	static void mfree(void *p);
	static void *QuickMapEnter(paddr_t pa);
	static void QuickMapRemove(vaddr_t va);

	MM();
};

#define ALLOC(type, count)		((type *)MM::malloc(sizeof(type) * count))

extern MM *mm;

#endif /* MM_H_ */
