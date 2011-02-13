/*
 * /phobos/gm.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#ifndef GM_H_
#define GM_H_
#include <sys.h>
phbSource("$Id$");

class GateObject;
class GApp;

class GM : public Object {
private:

public:
	class GateArea : public Object {
	public:
		enum {
			MAX_GATE_METHODS =	128,
		};
	private:
		class SlabClient : public SlabAllocator::SlabClient {
		private:
			GateArea *gateArea;
			SpinLock lock;
		public:
			SlabClient(GateArea *gateArea);
			virtual void *malloc(u32 size);
			virtual void mfree(void *p);
			virtual void FreeInitialPool(void *p, u32 size);
			virtual void Lock() { lock.Lock(); }
			virtual void Unlock() { lock.Unlock(); }
		};

		void *proc; /* PM::Process object */
		MM::Map *gateMap;
		GApp *app;
		SpinLock lock;
		ListHead objects, derefDelObjects;
		u32 numObjects;
		MM::VMObject *memObj;
		SlabClient slabClient;
		SlabAllocator slabAlloc;
		FUNC_PTR *gateVtable;
	public:
		GateArea(void *proc);
		~GateArea();

		int Initialize();
		GApp *GetApp(); /* must be called in the process context */
		void *AllocateArea(vsize_t size);
		void FreeArea(void *area);
		GateObject *CreateObject(vsize_t size);
		void FreeObject(GateObject *obj);
		vaddr_t Allocate(vsize_t size);
		int Free(vaddr_t addr);
		int RegisterObject(GateObject *obj);
		int UnregisterObject(GateObject *obj); /* schedule deletion */
		int IsMapped(vaddr_t va) { return gateMap->IsMapped(va); }
		FUNC_PTR *GetVtable() { return gateVtable; }
		MM::Map *GetMap() { return gateMap; }
	};

	class GClassRegistrator : public Object {
	private:
		const char *name;
		u32 id;
		Tree<u32>::TreeEntry tree;
		static Tree<u32>::TreeRoot classes;
	public:
		GClassRegistrator(const char *name, u32 *id);
		static const char *GetClassName(u32 id);
	};

	MM::VMObject *dispatchObj;
	u32 gateEntrySize;
	int dispatchInitialized;
	SpinLock dispatchLock;
public:
	GM();

	int InitCPU(); /* called by each CPU */
	GateArea *CreateGateArea(void *proc);
	FUNC_PTR *InitializeDispatcher(GateArea *gateArea);
	int FreeDispatcher(GateArea *gateArea);
};

extern GM *gm;

/* This macro must be used for new object creation */
#define GNEW(gateArea, className, ...) ({ \
	void *loc = (gateArea)->CreateObject(sizeof(className)); \
	if (loc) { \
		construct(loc, className, ## __VA_ARGS__); \
		if ((gateArea)->RegisterObject((GateObject *)loc)) { \
			delete ((className *)loc); \
			(gateArea)->FreeObject((GateObject *)loc); \
			loc = 0; \
		} \
	} \
	if (!loc) { \
		klog(KLOG_WARNING, "Failed to register gate object: %s (%lu bytes)", \
			__STR(className), sizeof(className)); \
	} \
	(className *)loc; \
})

#define GCLASS_ID(className) __CONCAT(className, _ID)
#define GCLASS_METHOD_T(className) __CONCAT(className, _method_t)

/* must precede class declaration */
#define DECLARE_GCLASS(className) \
	extern u32 GCLASS_ID(className);

/* must be included in class declaration AFTER all specific virtual methods */
#define DECLARE_GCLASS_IMP(className) \
	typedef void (className::*GCLASS_METHOD_T(className))(); \
	virtual u32 __GetID() { return GCLASS_ID(className); } \
	virtual u32 __GetSize() { return sizeof(className); } \
	virtual u32 __GetVtableSize() { return vtableSize; } \
	virtual u32 __CalculateVtableSize(); \
	virtual volatile void __CONCAT(className, _vtableEnd)() {};

/* must be included in class definition */
#define DEFINE_GCLASS(className) \
	u32 GCLASS_ID(className); \
	static GM::GClassRegistrator __UID(GClassRegistrator) \
		(__STR(className), &GCLASS_ID(className)); \
	u32 className::__CalculateVtableSize() \
	{ \
		volatile GCLASS_METHOD_T(className) m = \
			(GCLASS_METHOD_T(className)) \
			&__CONCAT(className::className, _vtableEnd); \
		ClassMethodPtr *mPtr = (ClassMethodPtr *)&m; \
		assert(mPtr->fn.vOffs & ClassMethodPtr::FF_VIRTUAL); \
		return vtableSize = (mPtr->fn.vOffs + sizeof(FUNC_PTR)) / \
			sizeof(FUNC_PTR); \
	}

#define IS_GCLASS_OBJ(obj, className) (obj->GetID() == GCLASS_ID(className))

#include <kern/pm.h>

DECLARE_GCLASS(GateObject);

class GateObject : public Object {
public:
	typedef enum {
		OP_READ,
		OP_WRITE,

		OP_MAX
	} Operation;
private:
	enum {
		GATE_OBJ_SIGNATURE = 'G' | ('A' << 8) | ('T' << 16) | ('E' << 24),
	};

	friend class GM::GateArea;
	ListEntry list;
	int isInitialized;
	u32 signature;
	GateObject *thisPtr; /* for pointer validation purpose */

	int ClearUserRef(); /* return last reference counter value */
	int Initialize();
protected:
	typedef struct {
		u64 numKernCalls, numUserCalls;
	} CallStatEntry;

	/* we have separate reference counters for user space and kernel */
	AtomicInt<u32> refCount, krefCount;
	PM::Process *proc;
	GM::GateArea *gateArea;
	u32 vtableSize;
	FUNC_PTR *origVtable;
	CallStatEntry *callStats;
	u64 numKernCalls, numUserCalls;

public:
	void operator delete(void *obj);
	GateObject();
	int KAddRef();
	int KRelease();
	static int Validate(GateObject *obj); /* return 0 if valid */
	inline FUNC_PTR GetOrigMethod(u32 idx) {
		if (idx >= vtableSize) {
			return 0;
		}
		return origVtable[idx];
	}
	void UpdateCallStat(u32 methodIdx, int userMode);
	inline const char *GetClassName() { return 0; } //notimpl

	virtual ~GateObject();
	virtual int AddRef();
	virtual int Release();

	/* Waitable objects must redefine these two methods */
	virtual PM::waitid_t GetWaitChannel(Operation op) { return 0; }
	/* return non-zero if available */
	virtual int OpAvailable(Operation op) { return 0; }

	DECLARE_GCLASS_IMP(GateObject);
};

ASMCALL FUNC_PTR GateObjGetOrigMethod(GateObject *obj, u32 idx);

ASMCALL FUNC_PTR GateObjValidateCall(u32 idx, vaddr_t esp);

ASMCALL FUNC_PTR GateObjGetReturnAddress();

#endif /* GM_H_ */
