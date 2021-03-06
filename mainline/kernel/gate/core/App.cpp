/*
 * /phobos/kernel/gate/App.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#include <sys.h>
phbSource("$Id$");

DEFINE_GCLASS(GApp);

GApp::GApp()
{
	gproc = GNEW(gateArea, GProcess);
	gtime = GNEW(gateArea, GTime);
	gvfs = GNEW(gateArea, GVFS);
}

GApp::~GApp()
{
	gvfs->Release();
	gtime->KRelease();
	gproc->KRelease();
}

void
GApp::ExitThread(int exitCode)
{
	PM::Thread::GetCurrent()->Exit(exitCode);
}

void
GApp::Abort(char *msg)
{
	PM::Thread *thrd = PM::Thread::GetCurrent();
	if (msg && !proc->CheckUserString(msg)) {
		thrd->Fault(PM::PFLT_ABORT, "%s", msg);
	} else {
		thrd->Fault(PM::PFLT_ABORT);
	}
}

int
GApp::Sleep(u64 time)
{
	return pm->Sleep(PM::Thread::GetCurrent(), "GApp::Sleep", time);
}

/*
 * Wait for multiple objects. The passed objects must support waiting for
 * requested operation. If bitmap pointer is provided then bits corresponding
 * for ready objects are set. The bitmap size should be multiple of sizeof(int).
 */
int
GApp::Wait(Operation op, GateObject **objects, int numObjects, void *bitmap,
	u64 timeout)
{
	if (!numObjects || numObjects < 0) {
		ERROR(E_INVAL, "Invalid number of objects (%d)", numObjects);
		return -1;
	}
	if (numObjects > MAX_WAIT_OBJECTS) {
		ERROR(E_INVAL, "Maximal number of objects exceeded (%d of %d)",
			numObjects, MAX_WAIT_OBJECTS);
		return -1;
	}
	if (op >= OP_MAX || op < 0) {
		ERROR(E_INVAL, "Invalid operation (%d)", op);
		return -1;
	}
	if (proc->CheckUserBuf(objects, numObjects * sizeof(objects[0]))) {
		return -1;
	}
	if (bitmap) {
		if (proc->CheckUserBuf(bitmap,
			roundup2(numObjects, NBBY * sizeof(int)) / NBBY)) {
			return -1;
		}
		memset(bitmap, 0, roundup2(numObjects, NBBY * sizeof(int)) / NBBY);
	}

	PM::waitid_t waitIds[numObjects];

	/* Validate all passed objects */
	for (int i = 0; i < numObjects; i++) {
		GateObject *obj = objects[i];
		if (GateObject::Validate(obj)) {
			ERROR(E_INVAL_GATEOBJ, "Invalid gate object: 0x%08lx", (vaddr_t)obj);
			return -1;
		}
		waitIds[i] = obj->GetWaitChannel(op);
		if (!waitIds[i]) {
			ERROR(E_OP_NOTSUPPORTED, "Wait of requested operation (%d) not "
				"supported by the object (#%d)", op, i);
			return -1;
		}
	}

	/* Reserve sleep channels */
	for (int i = 0; i < numObjects; i++) {
		pm->ReserveSleepChannel(waitIds[i]);
	}

	int rc;
	do {
		/* Check if we have operation available for any object */
		rc = 0;
		for (int i = 0; i < numObjects; i++) {
			if (objects[i]->OpAvailable(op)) {
				if (bitmap) {
					BitSet(bitmap, i);
				}
				rc++;
			}
		}
		if (rc) {
			break;
		}

		PM::waitid_t wakenBy;
		if (pm->Sleep(waitIds, numObjects, "App::Wait", timeout, &wakenBy)) {
			ERROR(E_FAULT, "PM::Sleep() call failed");
			break;
		}

		/* Do not even bother with objects scanning if we returned by timeout*/
		if (wakenBy) {
			for (int i = 0; i < numObjects; i++) {
				if (waitIds[i] == wakenBy || objects[i]->OpAvailable(op)) {
					if (bitmap) {
						BitSet(bitmap, i);
					}
					rc++;
				}
			}
		}
	} while (0);

	/* Free sleep channels */
	for (int i = 0; i < numObjects; i++) {
		pm->FreeSleepChannel(waitIds[i]);
	}
	return rc;
}

int
GApp::Wait(Operation op, GateObject *obj, u64 timeout)
{
	return Wait(op, &obj, 1, 0, timeout);
}

DEF_STR_PROV(GApp::GetLastErrorStr)
{
	if (buf && proc->CheckUserBuf(buf, bufLen, MM::PROT_WRITE)) {
		return -1;
	}
	/* get error object for previous system call */
	Error *e = PM::Thread::GetCurrent()->GetError(1);
	/* return to previous error object in order to not change history by this call */
	PM::Thread::GetCurrent()->PrevError();
	return e ? e->GetStr(buf, bufLen) : KString("No errors reported\n").Get(buf, bufLen);
}

Error::ErrorCode
GApp::GetLastError()
{
	/* get error object for previous system call */
	Error *e = PM::Thread::GetCurrent()->GetError(1);
	/* return to previous error object in order to not change history by this call */
	PM::Thread::GetCurrent()->PrevError();
	return e ? e->GetCode() : Error::E_SUCCESS;
}

GStream *
GApp::GetStream(const char *name)
{
	if (proc->CheckUserString(name)) {
		return 0;
	}
	GStream *stream = proc->GetStream(name);
	if (stream) {
		stream->AddRef();
	}
	return stream;
}

GProcess *
GApp::GetProcess()
{
	gproc->AddRef();
	return gproc;
}

GThread *
GApp::GetThread()
{
	GThread *thrd = GNEW(gateArea, GThread);
	if (!thrd) {
		ERROR(E_NOMEM, "Cannot allocate GThread object");
		return 0;
	}
	thrd->AddRef();
	return thrd;
}

GTime *
GApp::GetTime()
{
	gtime->AddRef();
	return gtime;
}

GVFS *
GApp::GetVFS()
{
	gvfs->AddRef();
	return gvfs;
}

void *
GApp::AllocateHeap(u32 size, int prot, void *location, int getZeroed)
{
	return proc->AllocateHeap(size, prot, location, getZeroed);
}

u32
GApp::GetHeapSize(void *p)
{
	return proc->GetHeapSize(p);
}

int
GApp::FreeHeap(void *p)
{
	return proc->FreeHeap(p);
}

void *
GApp::ReserveSpace(u32 size, vaddr_t va)
{
	return proc->ReserveSpace(size, va);
}

int
GApp::UnReserveSpace(void *p)
{
	return proc->UnReserveSpace(p);
}

GProcess *
GApp::CreateProcess(const char *path, const char *name, int priority,
	const char *args)
{
	if (proc->CheckUserString(path) || proc->CheckUserString(name)) {
		return 0;
	}
	PM::Process *newProc = pm->CreateProcess(path, name, priority, args);
	if (!newProc) {
		ERROR(E_FAULT, "Cannot create process");
		return 0;
	}
	GProcess *gproc = GNEW(gateArea, GProcess, newProc);
	if (!gproc) {
		ERROR(E_NOMEM, "Cannot create GProcess object");
		return 0;
	}
	gproc->AddRef();
	return gproc;
}

GEvent *
GApp::CreateEvent()
{
	GEvent *ev = GNEW(gateArea, GEvent);
	if (!ev) {
		ERROR(E_NOMEM, "Cannot allocate GEvent object");
		return 0;
	}
	ev->AddRef();
	return ev;
}
