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
 * 		+-------------------+ DEV_MEM_ADDRESS
 * 		| Buffers space		| BUF_SPACE_SIZE
 * 		+-------------------+ BUF_SPACE_ADDRESS (KERN_DYN_MEM_END)
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
#define BUF_SPACE_SIZE		(1 << 20)
#define BUF_SPACE_ADDRESS	(DEV_MEM_ADDRESS - BUF_SPACE_SIZE)
#define KERN_DYN_MEM_END	BUF_SPACE_ADDRESS
#define PTDPTDI				(PTMAP_ADDRESS >> PD_SHIFT)
#define APTDPTDI			(ALTPTMAP_ADDRESS >> PD_SHIFT)

#define ALLOC(type, count)		((type *)MM::malloc(sizeof(type) * (count)))
#define FREE(p)					MM::mfree(p)

#ifndef DEBUG_MALLOC
#define NEW(className,...)			new((int)0) className(__VA_ARGS__)
#define NEWSINGLE(className,...)	new(1) className(__VA_ARGS__)
#else /* DEBUG_MALLOC */
#define NEW(className,...)			new((int)0, __STR(className), __FILE__, __LINE__) className(__VA_ARGS__)
#define NEWSINGLE(className,...)	new(1, __STR(className), __FILE__, __LINE__) className(__VA_ARGS__)
#endif /* DEBUG_MALLOC */
#define DELETE(ptr)					delete (ptr)

void *operator new(size_t size, int isSingle);
void *operator new(size_t size, int isSingle, const char *className, const char *fileName, int line);
void operator delete(void *p);
void operator delete[](void *p);

class ConsoleDev;

class MM : public Object {
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

	enum Protection {
		PROT_NONE =		0x0,
		PROT_READ =		0x1,
		PROT_WRITE =	0x2,
		PROT_EXEC =		0x4,
		PROT_COW =		0x8,
	};

	enum PageFaultCode {
		PFC_P =			0x1,
		PFC_W =			0x2,
		PFC_U =			0x4,
		PFC_RSVD =		0x8,
		PFC_I =			0x10,
	};

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
	class Pager;

	class VMObject : public Object {
	public:
		enum Flags {
			F_FILE =		0x1,
			F_NOTPAGEABLE =	0x2,
			F_STACK =		0x4,
			F_HEAP =		0x8,
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
		RefCount	refCount;
		SpinLock	lock;
		Pager		*pager; /* backing storage */
		vaddr_t		pagerOffset;

	private:
		int CreateDefaultPager();
	public:

		VMObject(vsize_t size, u32 flags = 0);
		~VMObject();
		OBJ_ADDREF(refCount);
		OBJ_RELEASE(refCount);
		int SetSize(vsize_t size);
		inline vsize_t GetSize() { return size; }
		int InsertPage(Page *pg, vaddr_t offset);
		Page *LookupPage(vaddr_t offset);
		int CreatePager(Handle pagingHandle = 0);
		int Pagein(vaddr_t offset, Page **ppg = 0, int numPages = 1);
		int Pageout(vaddr_t offset, Page **ppg = 0, int numPages = 1);
	};

	VMObject *kmemObj, *devObj;

	enum PageAllocFlags {
		PAF_NOWAIT =		0x1,
	};

	enum PageZone {
		ZONE_1MB,
		ZONE_16MB,
		ZONE_4GB,
		ZONE_REST,
		NUM_ZONES,
	};

	class Page : public Object {
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
		vaddr_t		offset; /* offset in object */
		ListEntry	queue; /* entry in free, active, inactive or cached pages queue */
		/* entry in VMObject pages list, key is offset */
		Tree<vaddr_t>::TreeEntry	objEntry;

		Page(paddr_t pa, u16 flags);
		inline PageZone GetZone();
		int Unqueue(); /* must be called with pages queues locked */
		int Activate();
		int Wire();
	};

	ListHead pagesFree[NUM_ZONES], pagesCache[NUM_ZONES], pagesActive, pagesInactive;
	u32	numPgFree, numPgCache, numPgActive, numPgInactive, numPgWired;
	SpinLock pgqLock;

	class Map : public Object {
	public:
		class MapEntryAllocator;

		class Entry : public Object {
		public:
			enum Flags {
				F_SPACE =		0x1, /* space only allocation */
				F_RESERVE =		0x2, /* space reservation */
				F_NOCACHE =		0x4, /* disable caching */
				F_SUBMAP =		0x8, /* entry describes submap */
				F_USER =		0x10, /* accessible from user mode */
			};

			typedef struct {
				Entry *e;
				PageZone zone;
			}  EntryAllocParam;

			ListEntry			list;
			Map					*map;
			vaddr_t				base;
			vsize_t				size;
			union {
				VMObject		*object;
				Map				*submap;
			};
			vaddr_t				offset; /* offset in object */
			u32					flags;
			MapEntryAllocator	*alloc;
			int					protection;

			Entry(Map *map);
			~Entry();
			MemAllocator *CreateAllocator();
			int MapPage(vaddr_t va, Page *pg = 0);
			int MapPA(vaddr_t va, paddr_t pa);
			int Unmap(vaddr_t va);
			int GetOffset(vaddr_t va, vaddr_t *offs);
			int Pagein(vaddr_t va, int numPages = 1);
		};

		ListEntry	list; /* global maps list */
		vaddr_t		base;
		vsize_t		size;
		BuddyAllocator<vaddr_t> alloc; /* kernel virtual address space allocator */
		u32			numEntries;
		ListHead	entries;
		SpinLock	entriesLock; /* entries list lock */
		int			freeTables;
		PTE::PDEntry *pdpt; /* PDPT map in kernel KVAS */
		PTE::PDEntry *ptd; /* PTD map in kernel KVAS */
		SpinLock	tablesLock;
		SpinLock	smListLock;
		ListEntry	smList; /* submaps list */
		ListHead	submaps;
		Map			*parentMap, *rootMap;
		u32			cr3;
		Entry		*submapEntry;
		int			isUser; /* hint for newly created entries */

		class MapEntryClient : public BuddyAllocator<vaddr_t>::BuddyClient {
		private:
			MemAllocator *m;
			SpinLock lock;
		public:
			MapEntryClient(MemAllocator *m) { this->m = m; }
			virtual void *malloc(u32 size) { return m->malloc(size); }
			virtual void mfree(void *p) { return m->mfree(p); }
			virtual void *AllocateStruct(u32 size) {return m->AllocateStruct(size);}
			virtual void FreeStruct(void *p, u32 size) {return m->FreeStruct(p, size);}
			virtual int Allocate(vaddr_t base, vaddr_t size, void *arg = 0);
			virtual int Free(vaddr_t base, vaddr_t size, void *arg = 0);
			virtual int Reserve(vaddr_t base, vaddr_t size, void *arg = 0);
			virtual int UnReserve(vaddr_t base, vaddr_t size, void *arg = 0);
			virtual void Lock() { lock.Lock(); }
			virtual void Unlock() { lock.Unlock(); }
		};

		MapEntryClient	mapEntryClient;

		class MapEntryAllocator : public MemAllocator {
		public:
			enum {
				MIN_BLOCK =		4,
				MAX_BLOCK =		30,
			};
			RefCount refCount;
			Entry *e;
			BuddyAllocator<vaddr_t> alloc;

			MapEntryAllocator(Entry *e);
			virtual ~MapEntryAllocator();
			OBJ_ADDREF(refCount);
			OBJ_RELEASE(refCount);

			virtual void *malloc(u32 size, void *param);
			virtual void *malloc(u32 size) { return malloc(size, (void *)ZONE_REST); }
			virtual void mfree(void *p);
		};

		int Initialize();
		Entry *IntInsertObject(VMObject *obj, vaddr_t base = 0, int fixed = 0,
			vaddr_t offset = 0, vsize_t size = VSIZE_MAX,
			int protection = PROT_READ | PROT_WRITE);
		int FreeSubmaps(ListHead *submaps);
		int AddEntry(Entry *e);
		int DeleteEntry(Entry *e);
	public:
		Map(Map *copyFrom = 0);
		Map(PTE::PDEntry *pdpt, PTE::PDEntry *ptd, int noFree);
		~Map();
		int SetRange(vaddr_t base, vsize_t size,
			int minBlockOrder = KMEM_MIN_BLOCK, int maxBlockOrder = KMEM_MAX_BLOCK);

		Map *CreateSubmap(vaddr_t base, vsize_t size);
		int RemoveSubmap(Map *sm);
		inline Map *GetParent() { return parentMap; } /* return 0 for root map */
		Entry *Allocate(vsize_t size, vaddr_t *base, int fixed = 0);
		Entry *AllocateSpace(vsize_t size, vaddr_t *base = 0, int fixed = 0);
		int Free(vaddr_t base);
		int Free(Entry *e);
		Entry *ReserveSpace(vaddr_t base, vsize_t size);
		int UnReserveSpace(vaddr_t base);
		Entry *Lookup(vaddr_t base, int recursive = 0);
		Entry *InsertObject(VMObject *obj, vaddr_t offset = 0,
			vsize_t size = VSIZE_MAX, int protection = PROT_READ | PROT_WRITE);
		Entry *InsertObjectAt(VMObject *obj, vaddr_t base,
			vaddr_t offset = 0, vsize_t size = VSIZE_MAX,
			int protection = PROT_READ | PROT_WRITE);
		PTE::PDEntry *GetPDE(vaddr_t va);
		PTE::PTEntry *GetPTE(vaddr_t va);
		int IsMapped(vaddr_t va);
		paddr_t Extract(vaddr_t va);
		int IsCurrent();
		int IsAlt();
		void SetAlt(); /* set this map as current alternative AS */
		static void ResetAlt(); /* reset alternative AS mapping */
		int AddPT(vaddr_t va); /* must be called with locked tables */
		int Pagein(vaddr_t va); /* make this page resident */
		inline u32 GetCR3() { return cr3; }
		int AddPDE(vaddr_t va, PTE::PDEntry *pde);
	};

	Map *kmemMap; /* kernel process virtual address space */
	Map *bufMap; /* buffers space submap */

	class Pager : public Object {
	public:
		enum Type {
			T_SWAP,
			T_FILE,
			T_DEVICE,

			T_DEFAULT = T_SWAP
		};
	protected:
		RefCount	refCount;
		Type		type;
		u32			size;
		Handle		handle;

		Map::Entry *MapPages(Page **ppg, int numPages = 1);
		int UnmapPages(Map::Entry *e);
	public:
		Pager(Type type, vsize_t size, Handle handle = 0);
		virtual ~Pager();
		OBJ_ADDREF(refCount);
		OBJ_RELEASE(refCount);

		static Pager *CreatePager(Type type, vsize_t size, Handle handle = 0);

		inline Type GetType() { return type; }

		virtual int HasPage(vaddr_t offset) = 0;
		virtual int GetPage(vaddr_t offset, Page **ppg, int numPages = 1) = 0;
		virtual int PutPage(vaddr_t offset, Page **ppg, int numPages = 1) = 0;
	};
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
		OF_SINGLE =		0x2,
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
	int NXE; /* Execution disable bit in PTE available */
	int noNX;

	class KmemSlabClient : public SlabAllocator::SlabClient {
	private:
		SpinLock lock;
	public:
		virtual void *malloc(u32 size) { return MM::malloc(size); }
		virtual void mfree(void *p) { return MM::mfree(p); }
		virtual void FreeInitialPool(void *p, u32 size) {}
		virtual void Lock() { lock.Lock(); }
		virtual void Unlock() { lock.Unlock(); }
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
		virtual int Reserve(vaddr_t base, vaddr_t size, void *arg = 0);
		virtual int UnReserve(vaddr_t base, vaddr_t size, void *arg = 0);
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
		virtual int Reserve(paddr_t base, paddr_t size, void *arg = 0) { return 0; }
		virtual int UnReserve(paddr_t base, paddr_t size, void *arg = 0) { return 0; }
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
	SpinLock mapsLock;
	ListHead maps;

	static inline void FlushTLB() {wcr3(rcr3());}
	static void GrowMem(vaddr_t addr);
	static paddr_t _AllocPage(); /* at IS_MEMCOUNTED level */

	void InitAvailMem();
	void InitMM();
	const char *StrMemType(SMMemType type);
	int CreatePageDescs();
	Page *GetFreePage(int zone = ZONE_REST); /* pages queues must be locked */
	Page *GetCachedPage(PageZone zone = ZONE_REST); /* pages queues must be locked */
	int OnPageFault(Frame *frame);
	int HandlePageFault(vaddr_t va, u32 code, int isUserMode); /* ret zero if handled */
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
	static void *malloc(u32 size, u32 align = 4, PageZone zone = ZONE_REST);
	static void mfree(void *p);
	static void *QuickMapEnter(paddr_t pa);
	static void QuickMapRemove(vaddr_t va);
	static void ZeroPage(paddr_t pa);

	MM(); /* XXX destructor required */
	Page *AllocatePage(int flags = 0, PageZone zone = ZONE_REST);
	Page *GetPage(paddr_t pa);
	int FreePage(Page *pg);
	paddr_t Kextract(vaddr_t va); /* in kernel AS */
	vaddr_t MapDevPhys(paddr_t pa, psize_t size); /* map in devices memory region, can be unmapped by mfree() */
	int PrintMemInfo(ConsoleDev *dev);
	paddr_t AllocDevPhys(psize_t size); /* allocate physical memory in devices region */
	int FreeDevPhys(paddr_t addr);
	int MapPhys(vaddr_t va, paddr_t pa);
	int UnmapPhys(vaddr_t va);
	Map *CreateMap();
	int DestroyMap(Map *map);
	int UpdatePDE(Map *originator, vaddr_t va, PTE::PDEntry *pde);
	int AttachCPU();
};

extern MM *mm;

#endif /* MM_H_ */
