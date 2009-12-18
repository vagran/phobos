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
 * 		+-------------------+ ALTPTMAP_ADDRESS = FF000000 (KERNEL_END_ADDRESS)
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
#define KERNEL_END_ADDRESS	ALTPTMAP_ADDRESS

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

	MemChunk physMem[MEM_MAX_CHUNKS];
	u32 physMemSize;
	MemChunk availMem[MEM_MAX_CHUNKS];
	u32 availMemSize;
	u32 totalMem;
	SlabAllocator *kmemSlab;
	ListHead objects; /* all objects */
	ListHead topObjects; /* top-level objects */
	u32 numObjects;

	class VMObject {
	public:
		enum Flags {
			F_FILE =		0x1,
		};

		ListEntry	list; /* list of all objects */
		vsize_t		size;
		u32			flags;
		ListHead	pages; /* resident pages list */
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

		VMObject(vsize_t size);
		~VMObject();
		OBJ_ADDREF(refCount);
		OBJ_RELEASE(refCount);
		int SetSize(vsize_t size);
	};

	VMObject *kmemObj;

	class Page {
	public:
		enum Flags {
			F_FREE =		0x1,
			F_ACTIVE =		0x2,
			F_INACTIVE =	0x4,
			F_CACHE =		0x8,
			F_NOTAVAIL =	0x10,
		};

		paddr_t		pa;
		u16			flags;
		u16			wireCount;
		VMObject	*object;
		vaddr_t		offset; /* offset in object */
		ListEntry	queue; /* entry in free, active, inactive or cached pages queue */
		ListEntry	objList; /*entry in VMObject pages list */

		Page(paddr_t pa, u16 flags);
	};

	class Map {
	public:

		vaddr_t		base;
		vsize_t		size;
		BuddyAllocator<vaddr_t> alloc; /* kernel virtual address space allocator */
		u32			numEntries;
		ListHead	entries;
		Mutex		entriesLock; /* entries list lock */

		class MapEntryAllocator;

		class Entry {
		public:
			enum Flags {
				F_SPACE =		0x1, /* space only allocation */
				F_RESERVE =		0x2, /* space reservation */
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

	public:
		Map();
		~Map();
		int SetRange(vaddr_t base, vsize_t size, int minBlockOrder = 4, int maxBlockOrder = 30);

		Entry *Allocate(vsize_t size, vaddr_t *base, int fixed = 0);
		Entry *AllocateSpace(vsize_t size, vaddr_t *base, int fixed = 0);
		Entry *ReserveSpace(vaddr_t base, vsize_t size);
		Entry *Lookup(vaddr_t base);
		int Free(vaddr_t base);
		Entry *InsertObject(VMObject *obj);
		Entry *InsertObject(VMObject *obj, vaddr_t base);
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
		KMEM_SLAB_INITIALMEM_SIZE = 128 * 1024,
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
		virtual int Allocate(vaddr_t base, vaddr_t size, void *arg = 0);
		virtual int Free(vaddr_t base, vaddr_t size, void *arg = 0);
		virtual void Lock() { mtx.Lock(); }
		virtual void Unlock() { mtx.Unlock(); }
	};

	KmemSlabClient *kmemSlabClient;
	void *kmemSlabInitialMem;
	KmemMapClient *kmemMapClient;
	Map::Entry *kmemEntry;
	MemAllocator *kmemAlloc;

	static inline void FlushTLB() {wcr3(rcr3());}
	static void GrowMem(vaddr_t addr);
	static paddr_t _AllocPage(); /* at IS_MEMCOUNTED level */

	void InitAvailMem();
	void InitMM();
	const char *StrMemType(SMMemType type);
	int CreatePageDescs();
public:
	static void PreInitialize(vaddr_t addr);
	static paddr_t VtoP(vaddr_t va);
	static inline PTE::PDEntry *VtoPDE(vaddr_t va) {return &PTD[va >> PD_SHIFT];}
	static inline PTE::PTEntry *VtoPTE(vaddr_t va) {return &PTmap[va >> PT_SHIFT];}
	static inline void *OpNew(u32 size, int isSingle);
	static inline void *OpNew(u32 size, int isSingle, const char *className, const char *fileName, int line);
	static inline void OpDelete(void *p);
	static void *malloc(u32 size, u32 align = 4);
	static void mfree(void *p);
	static void *QuickMapEnter(paddr_t pa);
	static void QuickMapRemove(vaddr_t va);

	MM();
};

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

extern MM *mm;

#endif /* MM_H_ */
