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

/*
 * Virtual memory map:
 * 		--------------------- FFFFFFFF
 * 		| PT map			| PD_PAGES * PT_ENTRIES * PAGE_SIZE
 * 		+-------------------+ PTMAP_ADDRESS = FF800000
 * 		| Alt. PT map		| PD_PAGES * PT_ENTRIES * PAGE_SIZE
 * 		+-------------------+ ALTPTMAP_ADDRESS = FF000000 (KERN_MEM_END)
 * 		| Devices memory	| DEV_MEM_SIZE
 * 		+-------------------+ DEV_MEM_ADDRESS (KERN_DYN_MEM_END)
 * 		| Kernel dynamic 	|
 * 		| memory			|
 * 		+-------------------+ firstAddr
 * 		| Kernel image,		|
 * 		| initial memory	|
 * 		+-------------------+ KERNEL_ADDRESS
 * 		| Gate objects		| GATE_AREA_SIZE
 * 		+-------------------+ GATE_AREA_ADDRESS
 * 		| Stack				|
 * 		|					|
 * 		.....................
 * 		| Process dynamic	|
 * 		| memory			|
 * 		+-------------------+
 * 		| Process data		|
 * 		+-------------------+
 * 		| Process code		|
 * 		+-------------------+ PAGE_SIZE
 * 		| Guard page		|
 * 		+-------------------+ 00000000
 *
 */

#define GATE_AREA_SIZE		(64 << 20)
#define GATE_AREA_ADDRESS	(KERNEL_ADDRESS - GATE_AREA_SIZE)
#define PTMAP_SIZE			(PD_PAGES * PT_ENTRIES * PAGE_SIZE)
#define PTMAP_ADDRESS		((vaddr_t)-PTMAP_SIZE)
#define ALTPTMAP_ADDRESS	(PTMAP_ADDRESS - PTMAP_SIZE)
#define KERN_MEM_END		ALTPTMAP_ADDRESS
#define DEV_MEM_SIZE		(64 << 20)
#define DEV_MEM_ADDRESS		(ALTPTMAP_ADDRESS - DEV_MEM_SIZE)
#define KERN_DYN_MEM_END	DEV_MEM_ADDRESS
#define PTDPTDI				(PTMAP_ADDRESS >> PD_SHIFT)
#define APTDPTDI			(ALTPTMAP_ADDRESS >> PD_SHIFT)

#define ALLOC(type, count)		((type *)MM::malloc(sizeof(type) * (count)))

#ifndef DEBUG_MALLOC
#define NEW(className,...)			new((int)0) className(__VA_ARGS__)
#define NEWSINGLE(className,...)	new(1) className(__VA_ARGS__)
#else /* DEBUG_MALLOC */
#define NEW(className,...)			new((int)0, __STR(className), __FILE__, __LINE__) className(__VA_ARGS__)
#define NEWSINGLE(className,...)	new(1, __STR(className), __FILE__, __LINE__) className(__VA_ARGS__)
#endif /* DEBUG_MALLOC */

void *operator new(size_t size, int isSingle);
void *operator new(size_t size, int isSingle, const char *className, const char *fileName, int line);

#define DELETE(ptr)				delete (ptr)

class MM {
public:
	enum {
		QUICKMAP_SIZE =		8, /* pages */
		MEM_MAX_CHUNKS =	16,
		KMEM_MIN_BLOCK =	4, /* bits shift */
		KMEM_MAX_BLOCK =	30,
	};

	typedef struct {
		paddr_t	start, end;
	} MemChunk;

	typedef struct {
		paddr_t location;
		psize_t size;
		SMMemType type;
	} BiosMemChunk;

	MemChunk physMem[MEM_MAX_CHUNKS];
	u32 physMemSize;
	psize_t physMemTotal;
	MemChunk availMem[MEM_MAX_CHUNKS];
	u32 availMemSize;
	BiosMemChunk biosMem[MEM_MAX_CHUNKS];
	u32 biosMemSize;
	u32 totalMem;
	paddr_t devPhysMem; /* physical memory space for memory-mapped devices */
	SlabAllocator *kmemSlab;
	ListHead objects; /* all objects */
	ListHead topObjects; /* top-level objects */
	u32 numObjects;

	class Page;

	class VMObject {
	public:
		enum Flags {
			F_FILE =		0x1,
			F_NOTPAGEABLE =	0x2,
		};

		ListEntry	list; /* list of all objects */
		vsize_t		size;
		u32			flags;
		Tree<vaddr_t>::TreeRoot		pages; /* resident pages tree */
		u32			numPages; /* resident pages count */
		ListHead	shadowObj; /* shadow objects */
		/* list of shadow objects in copy object,
		 * when copyObj == 0 (the object is not a shadow), this is
		 * entry in list of top-level objects.
		 */
		ListEntry	shadowList;
		VMObject	*copyObj; /* object to copy changed pages from */
		vaddr_t		copyOffset; /* offset in copy object */
		u32			refCount;
		SpinLock	lock;

		VMObject(vsize_t size);
		~VMObject();
		OBJ_ADDREF(refCount);
		OBJ_RELEASE(refCount);
		int SetSize(vsize_t size);
		int InsertPage(Page *pg, vaddr_t offset);
		Page *LookupPage(vaddr_t offset);
	};

	VMObject *kmemObj, *devObj;

	enum PageAllocFlags {
		PAF_NOWAIT =		0x1,
	};

	enum PageZones {
		ZONE_1MB,
		ZONE_16MB,
		ZONE_4GB,
		ZONE_REST,
		NUM_ZONES,
	};

	class Page {
	public:
		enum Flags {
			F_ZONEMASK =	0x000f,
			F_FREE =		0x0010,
			F_ACTIVE =		0x0020,
			F_INACTIVE =	0x0040,
			F_CACHE =		0x0080,
			F_NOTAVAIL =	0x0100,
		};

		paddr_t		pa;
		u16			flags;
		u16			wireCount;
		VMObject	*object;
		ListEntry	queue; /* entry in free, active, inactive or cached pages queue */
		/* entry in VMObject pages list, key is offset */
		Tree<vaddr_t>::TreeEntry	objEntry;

		Page(paddr_t pa, u16 flags);
		inline PageZones GetZone();
		int Unqueue(); /* must be called with pages queues locked */
		int Activate();
		int Wire();
	};

	ListHead pagesFree[NUM_ZONES], pagesCache[NUM_ZONES], pagesActive, pagesInactive;
	u32	numPgFree, numPgCache, numPgActive, numPgInactive, numPgWired;
	SpinLock pgqLock;

	class Map {
	public:

		vaddr_t		base;
		vsize_t		size;
		BuddyAllocator<vaddr_t> alloc; /* kernel virtual address space allocator */
		u32			numEntries;
		ListHead	entries;
		Mutex		entriesLock; /* entries list lock */
		int			freeTables;
		PTE::PDEntry *pdpt; /* PDPT map in kernel KVAS */
		PTE::PDEntry *ptd; /* PTD map in kernel KVAS */
		SpinLock	tablesLock;

		class MapEntryAllocator;

		class Entry {
		public:
			enum Flags {
				F_SPACE =		0x1, /* space only allocation */
				F_RESERVE =		0x2, /* space reservation */
				F_NOCACHE =		0x4, /* disable caching */
			};

			ListEntry			list;
			Map					*map;
			vaddr_t				base;
			vsize_t				size;
			VMObject			*object;
			vaddr_t				offset; /* offset in object */
			u32					flags;
			MapEntryAllocator	*alloc;

			Entry(Map *map);
			~Entry();
			MemAllocator *CreateAllocator();
			int MapPage(vaddr_t va, Page *pg = 0);
			int MapPA(vaddr_t va, paddr_t pa);
			int Unmap(vaddr_t va);
		};

		int AddEntry(Entry *e);
		int DeleteEntry(Entry *e);

		class MapEntryClient : public BuddyAllocator<vaddr_t>::BuddyClient {
		private:
			MemAllocator *m;
			Mutex mtx;
		public:
			MapEntryClient(MemAllocator *m) { this->m = m; }
			virtual void *malloc(u32 size) { return m->malloc(size); }
			virtual void mfree(void *p) { return m->mfree(p); }
			virtual void *AllocateStruct(u32 size) {return m->AllocateStruct(size);}
			virtual void FreeStruct(void *p, u32 size) {return m->FreeStruct(p, size);}
			virtual int Allocate(vaddr_t base, vaddr_t size, void *arg = 0);
			virtual int Free(vaddr_t base, vaddr_t size, void *arg = 0);
			virtual void Lock() { mtx.Lock(); }
			virtual void Unlock() { mtx.Unlock(); }
		};

		MapEntryClient	mapEntryClient;

		class MapEntryAllocator : public MemAllocator {
		public:
			enum {
				MIN_BLOCK =		4,
				MAX_BLOCK =		30,
			};
			u32 refCount;
			Entry *e;
			BuddyAllocator<vaddr_t> alloc;

			MapEntryAllocator(Entry *e);
			virtual ~MapEntryAllocator();
			OBJ_ADDREF(refCount);
			OBJ_RELEASE(refCount);

			virtual void *malloc(u32 size);
			virtual void mfree(void *p);
		};

		int Initialize();
		Entry *IntInsertObject(VMObject *obj, vaddr_t base = 0, int fixed = 0,
			vaddr_t offset = 0, vsize_t size = VSIZE_MAX);
	public:
		Map(Map *copyFrom = 0);
		Map(PTE::PDEntry *pdpt, PTE::PDEntry *ptd, int noFree);
		~Map();
		int SetRange(vaddr_t base, vsize_t size, int minBlockOrder = 4, int maxBlockOrder = 30);

		Entry *Allocate(vsize_t size, vaddr_t *base, int fixed = 0);
		Entry *AllocateSpace(vsize_t size, vaddr_t *base = 0, int fixed = 0);
		Entry *ReserveSpace(vaddr_t base, vsize_t size);
		Entry *Lookup(vaddr_t base);
		int Free(vaddr_t base);
		Entry *InsertObject(VMObject *obj, vaddr_t offset = 0, vsize_t size = VSIZE_MAX);
		Entry *InsertObjectAt(VMObject *obj, vaddr_t base,
			vaddr_t offset = 0, vsize_t size = VSIZE_MAX);
		PTE::PDEntry *GetPDE(vaddr_t va);
		PTE::PTEntry *GetPTE(vaddr_t va);
		paddr_t Extract(vaddr_t va);
		int IsCurrent();
		int IsAlt();
		void SetAlt(); /* set this map as current alternative AS */
		int AddPT(vaddr_t va); /* must be called with locked tables */
	};

	Map *kmemMap;
private:
	typedef enum {
		IS_INITIAL,
		IS_MEMCOUNTED,
		IS_INITIALIZING,
		IS_NORMAL,
	} InitState;

	enum {
		KMEM_SLAB_INITIALMEM_SIZE = 64 * 1024,
	};

	typedef enum {
		OF_NOFREE =		0x1,
	} ObjFlags;

	typedef struct {
		u32 flags;
#ifdef DEBUG_MALLOC
		const char *className;
		const char *fileName;
		int line;
#endif /* DEBUG_MALLOC */
	} ObjOverhead;

	static vaddr_t firstAddr;
	static PTE::PTEntry *PTmap, *altPTmap;
	static PTE::PDEntry *PTD, *PTDpde, *altPTD, *altPTDpde;
	static PTE::PTEntry *quickMapPTE;
	static InitState initState;
	Page *pages;
	u32 pagesRange, firstPage;

	class KmemSlabClient : public SlabAllocator::SlabClient {
	private:
		Mutex mtx;
	public:
		virtual void *malloc(u32 size) { return MM::malloc(size); }
		virtual void mfree(void *p) { return MM::mfree(p); }
		virtual void FreeInitialPool(void *p, u32 size) {}
		virtual void Lock() { mtx.Lock(); }
		virtual void Unlock() { mtx.Unlock(); }
	};

	class KmemMapClient : public BuddyAllocator<vaddr_t>::BuddyClient {
	private:
		MemAllocator *m;
		Mutex mtx;
	public:
		KmemMapClient(MemAllocator *m) { this->m = m; }
		virtual void *malloc(u32 size) { return m->malloc(size); }
		virtual void mfree(void *p) { return m->mfree(p); }
		virtual void *AllocateStruct(u32 size) {return m->AllocateStruct(size);}
		virtual void FreeStruct(void *p, u32 size) {return m->FreeStruct(p, size);}
		virtual int Allocate(vaddr_t base, vsize_t size, void *arg = 0);
		virtual int Free(vaddr_t base, vsize_t size, void *arg = 0);
		virtual void Lock() { mtx.Lock(); }
		virtual void Unlock() { mtx.Unlock(); }
	};

	class PABuddyClient : public BuddyAllocator<paddr_t>::BuddyClient {
	private:
		MemAllocator *m;
		Mutex mtx;
	public:
		PABuddyClient(MemAllocator *m) { this->m = m; }
		virtual void *malloc(u32 size) { return m->malloc(size); }
		virtual void mfree(void *p) { return m->mfree(p); }
		virtual void *AllocateStruct(u32 size) {return m->AllocateStruct(size);}
		virtual void FreeStruct(void *p, u32 size) {return m->FreeStruct(p, size);}
		virtual int Allocate(paddr_t base, psize_t size, void *arg = 0) { return 0; }
		virtual int Free(paddr_t base, psize_t size, void *arg = 0) { return 0; }
		virtual void Lock() { mtx.Lock(); }
		virtual void Unlock() { mtx.Unlock(); }
	};

	KmemSlabClient *kmemSlabClient;
	void *kmemSlabInitialMem;
	KmemMapClient *kmemMapClient;
	PABuddyClient *devMemClient;
	Map::Entry *kmemEntry, *devEntry;
	MemAllocator *kmemAlloc, *devAlloc;
	BuddyAllocator<paddr_t> *devMemAlloc;

	static inline void FlushTLB() {wcr3(rcr3());}
	static void GrowMem(vaddr_t addr);
	static paddr_t _AllocPage(); /* at IS_MEMCOUNTED level */

	void InitAvailMem();
	void InitMM();
	const char *StrMemType(SMMemType type);
	int CreatePageDescs();
	Page *GetFreePage(int zone = ZONE_REST); /* pages queues must be locked */
	Page *GetCachedPage(PageZones zone = ZONE_REST); /* pages queues must be locked */
public:
	static void PreInitialize(vaddr_t addr);
	static paddr_t VtoP(vaddr_t va); /* in current AS */
	static inline PTE::PDEntry *VtoPDE(vaddr_t va) {return &PTD[va >> PD_SHIFT];}
	static inline PTE::PDEntry *VtoAPDE(vaddr_t va) {return &altPTD[va >> PD_SHIFT];}
	static inline PTE::PTEntry *VtoPTE(vaddr_t va) {return &PTmap[va >> PT_SHIFT];}
	static inline PTE::PTEntry *VtoAPTE(vaddr_t va) {return &altPTmap[va >> PT_SHIFT];}
	static inline void *OpNew(u32 size, int isSingle);
	static inline void *OpNew(u32 size, int isSingle, const char *className, const char *fileName, int line);
	static inline void OpDelete(void *p);
	static void *malloc(u32 size, u32 align = 4);
	static void mfree(void *p);
	static void *QuickMapEnter(paddr_t pa);
	static void QuickMapRemove(vaddr_t va);
	static void ZeroPage(paddr_t pa);

	MM();
	Page *AllocatePage(int flags = 0, PageZones zone = ZONE_REST);
	Page *GetPage(paddr_t pa);
	int FreePage(Page *pg);
	paddr_t Kextract(vaddr_t va); /* in kernel AS */
	vaddr_t MapDevPhys(paddr_t pa, psize_t size); /* map in devices memory region, can be unmapped by mfree() */
	int PrintMemInfo(ConsoleDev *dev);
	paddr_t AllocDevPhys(psize_t size); /* allocate physical memory in devices region */
	int FreeDevPhys(paddr_t addr);
	int MapPhys(vaddr_t va, paddr_t pa);
	int UnmapPhys(vaddr_t va);
};

extern MM *mm;

#endif /* MM_H_ */
