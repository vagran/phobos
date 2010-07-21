/*
 * /phobos/kernel/kern/gm.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <gate/gate.h>

GM *gm;

GM::GM()
{
	gateEntrySize = roundup2((u32)&GateEntryEnd - (u32)&GateEntryStart, 4);
	/* create dispatch tables backing object */
	u32 dispatchSize = gateEntrySize * GateArea::MAX_GATE_METHODS +
		sizeof(FUNC_PTR) * (GateArea::MAX_GATE_METHODS + 1);
	dispatchSize = roundup2(dispatchSize, PAGE_SIZE);
	dispatchObj = NEW(MM::VMObject, dispatchSize, MM::VMObject::F_GATE);
	assert(dispatchObj);
	/* make all pages resident */
	dispatchObj->Pagein(0, 0, dispatchSize / PAGE_SIZE);
	dispatchInitialized = 0;
}

/* return dispatcher vtable */
FUNC_PTR *
GM::InitializeDispatcher(GateArea *gateArea)
{
	u8 *dispatchTable = (u8 *)GATE_AREA_ADDRESS;
	FUNC_PTR *gateVtable = (FUNC_PTR *)(dispatchTable +
		gateEntrySize * GateArea::MAX_GATE_METHODS);
	MM::Map::Entry *e = gateArea->GetMap()->InsertObjectAt(dispatchObj,
		GATE_AREA_ADDRESS, 0, VSIZE_MAX, MM::PROT_READ | MM::PROT_EXEC);
	if (!e) {
		return 0;
	}
	/* fill tables if not yet filled */
	dispatchLock.Lock();
	if (!dispatchInitialized) {
		for (u32 i = 0; i < GateArea::MAX_GATE_METHODS; i++) {
			FUNC_PTR entry = (FUNC_PTR)&dispatchTable[i * gateEntrySize];
			memcpy((void *)entry, &GateEntryStart, gateEntrySize);
			gateVtable[i] = entry;
		}
		gateVtable[GateArea::MAX_GATE_METHODS] = 0;
		dispatchInitialized = 1;
	}
	dispatchLock.Unlock();
	return gateVtable;
}

int
GM::FreeDispatcher(GateArea *gateArea)
{
	gateArea->GetMap()->Free(GATE_AREA_ADDRESS);
	return 0;
}

int
GM::InitCPU()
{
	/* establish kernel entry point */
	wrmsr(MSR_SYSENTER_CS_MSR, gdt->GetSelector(GDT::SI_KCODE));
	wrmsr(MSR_SYSENTER_EIP_MSR, (u64)&GateSysEntry);
	return 0;
}

GM::GateArea *
GM::CreateGateArea(void *proc)
{
	return NEW(GateArea, proc);
}

/* GM::GateArea class */
GM::GateArea::GateArea(void *proc) : slabClient(this), slabAlloc(&slabClient)
{
	this->proc = proc;
	gateMap = ((PM::Process *)proc)->GetGateMap();
	assert(gateMap);
	app = 0;
	LIST_INIT(objects);
	LIST_INIT(derefDelObjects);
	numObjects = 0;
	memObj = NEW(MM::VMObject, GATE_AREA_SIZE, MM::VMObject::F_GATE);
	assert(memObj);
	gateVtable = 0;
}

GM::GateArea::~GateArea()
{
	/* release all objects */
	lock.Lock();
	GateObject *obj = LIST_FIRST(GateObject, list, objects);
	lock.Unlock();
	while (obj) {
		lock.Lock();
		GateObject *nextObj = LIST_ISLAST(list, obj, objects) ? 0 :
			LIST_NEXT(GateObject, list, obj);
		lock.Unlock();
		obj->ClearUserRef();
		obj->KRelease();
		obj = nextObj;
	}
	assert(LIST_ISEMPTY(objects));
	assert(!numObjects);
	if (gateVtable) {
		gm->FreeDispatcher(this);
		gateVtable = 0;
	}
	memObj->Release();
}

/* Allocate memory region in gate area */
vaddr_t
GM::GateArea::Allocate(vsize_t size)
{
	size = roundup2(size, PAGE_SIZE);
	MM::Map::Entry *e = gateMap->InsertObjectOffs(memObj, size, 0,
		MM::PROT_READ | MM::PROT_EXEC);
	if (!e) {
		return 0;
	}
	return e->base;
}

/* Free region allocated by Allocate() method */
int
GM::GateArea::Free(vaddr_t addr)
{
	return gateMap->Free(addr);
}

GM::GateArea::SlabClient::SlabClient(GateArea *gateArea)
{
	this->gateArea = gateArea;
}

void *
GM::GateArea::SlabClient::malloc(u32 size)
{
	return (void *)gateArea->Allocate(size);
}

void
GM::GateArea::SlabClient::mfree(void *p)
{
	gateArea->Free((vaddr_t)p);
}

void
GM::GateArea::SlabClient::FreeInitialPool(void *p, u32 size)
{
	mfree(p);
}

void *
GM::GateArea::AllocateArea(vsize_t size)
{
	return slabAlloc.AllocateStruct(size);
}

void
GM::GateArea::FreeArea(void *area)
{
	slabAlloc.FreeStruct(area);
}

GateObject *
GM::GateArea::CreateObject(vsize_t size)
{
	GateObject *obj = (GateObject *)AllocateArea(size);
	if (!obj) {
		return 0;
	}
	return obj;
}

void
GM::GateArea::FreeObject(GateObject *obj)
{
	FreeArea(obj);
}

int
GM::GateArea::RegisterObject(GateObject *obj)
{
	if (obj->Initialize()) {
		return -1;
	}
	lock.Lock();
	LIST_ADD(list, obj, objects);
	numObjects++;
	lock.Unlock();
	return 0;
}

int
GM::GateArea::UnregisterObject(GateObject *obj)
{
	lock.Lock();
	LIST_DELETE(list, obj, objects);
	assert(numObjects);
	numObjects--;
	/* schedule object deletion */
	LIST_ADD(list, obj, derefDelObjects);
	lock.Unlock();
	return 0;
}

GApp *
GM::GateArea::GetApp()
{
	/* must be called in the process context */
	assert((PM::Process *)proc == PM::Process::GetCurrent());
	if (!app) {
		app = GNEW(this, GApp);
		assert(app);
	}
	return app;
}

/* must be called in context of the process */
int
GM::GateArea::Initialize()
{
	gateVtable = gm->InitializeDispatcher(this);
	return 0;
}

/* GM::GClassRegistrator class */

Tree<u32>::TreeRoot GM::GClassRegistrator::classes;

GM::GClassRegistrator::GClassRegistrator(const char *name, u32 *id)
{
	this->name = name;
	this->id = gethash(name);
	assert(!TREE_FIND(this->id, GClassRegistrator, tree, classes));
	TREE_ADD(tree, this, classes, this->id);
	if (id) {
		*id = this->id;
	}
}

const char *
GM::GClassRegistrator::GetClassName(u32 id)
{
	GClassRegistrator *r = TREE_FIND(id, GClassRegistrator, tree, classes);
	return r ? r->name : 0;
}

/* GateObject class */

DEFINE_GCLASS(GateObject);

GateObject::GateObject()
{
	refCount = 0;
	krefCount = 1;
	proc = PM::Process::GetCurrent();
	gateArea = proc->GetGateArea();
	origVtable = 0;
	isInitialized = 0;
	signature = GATE_OBJ_SIGNATURE;
	thisPtr = this;
}

GateObject::~GateObject()
{
	/* destructor should never be called from user-land */

	if (isInitialized) {
		gateArea->UnregisterObject(this);
	}
}

void
GateObject::operator delete(void *obj)
{
	/* the standard kernel delete operator should not be called */
}

int
GateObject::AddRef()
{
	++refCount;
	return (int)refCount;
}

int
GateObject::Release()
{
	if (!refCount) {
		/* XXX send signal */
		return -1;
	}
	int rc = --refCount;
	if (!rc && !krefCount) {
		DELETE(this);
	}
	return rc;
}

int
GateObject::KAddRef()
{
	++krefCount;
	return (int)krefCount;
}

int
GateObject::KRelease()
{
	assert(krefCount);
	int rc = --krefCount;
	if (!rc && !refCount) {
		DELETE(this);
	}
	return rc;
}

int
GateObject::Validate(GateObject *obj)
{
	GM::GateArea *gateArea = PM::Process::GetCurrent()->GetGateArea();

	vaddr_t start_va = rounddown2((vaddr_t)obj, PAGE_SIZE);
	vaddr_t end_va = (vaddr_t)obj + sizeof(GateObject);
	if (start_va < GATE_AREA_ADDRESS ||
		end_va >= GATE_AREA_ADDRESS + GATE_AREA_SIZE) {
		return -1;
	}
	for (vaddr_t va = start_va; va < end_va; va += PAGE_SIZE) {
		if (!gateArea->IsMapped(va)) {
			return -1;
		}
	}
	return obj->signature == GATE_OBJ_SIGNATURE && obj->thisPtr == obj;
}

int
GateObject::ClearUserRef()
{
	int rc = refCount;
	refCount = 0;
	return rc;
}

ASMCALL int
GateEntry(void *entryAddr, GateObject *obj)
{
	/* entered in user mode */
	if (GateObject::Validate(obj)) {
		PM::Process::GetCurrent()->Fault(PM::PFLT_GATEOBJ);
	}
	return 0;
}

int
GateObject::Initialize()
{
	CalculateVtableSize();
	origVtable = VTABLE(this);
	VTABLE(this) = gateArea->GetVtable();;
	isInitialized = 1;
	return 0;
}
