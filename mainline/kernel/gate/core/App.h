/*
 * /phobos/kernel/gate/App.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#ifndef APP_H_
#define APP_H_
#include <sys.h>
phbSource("$Id$");

#include <gate/core/GProcess.h>
#include <gate/io/GateStream.h>
#include <gate/core/GTime.h>
#include <gate/core/GVFS.h>
#include <gate/core/GEvent.h>

DECLARE_GCLASS(GApp);

class GApp : public GateObject {
private:
	enum {
		MAX_WAIT_OBJECTS =		1024, /* Maximal number of object for Wait() method */
	};

	GProcess *gproc;
	GTime *gtime;
	GVFS *gvfs;
public:
	GApp();
	virtual ~GApp();

	virtual void ExitThread(int exitCode = 0) __noreturn;
	virtual void Abort(char *msg = 0);
	virtual int Sleep(u64 time);
	/* Return -1 if error or number of alerted objects */
	virtual int Wait(Operation op, GateObject **objects, int numObjects,
		void *bitmap = 0, u64 timeout = 0);
	/* Return -1 if error, 0 if timeout, 1 if object alerted */
	virtual int Wait(Operation op, GateObject *obj, u64 timeout = 0);

	virtual DECL_STR_PROV(GetLastErrorStr);
	virtual Error::ErrorCode GetLastError();

	virtual GProcess *GetProcess();
	virtual GThread *GetThread();
	virtual GStream *GetStream(const char *name);
	virtual GTime *GetTime();
	virtual GVFS *GetVFS();

	virtual void *AllocateHeap(u32 size, int prot = MM::PROT_READ | MM::PROT_WRITE,
		void *location = 0, int getZeroed = 0);
	virtual u32 GetHeapSize(void *p);
	virtual int FreeHeap(void *p);
	virtual void *ReserveSpace(u32 size, vaddr_t va = 0);
	virtual int UnReserveSpace(void *p);

	virtual GProcess *CreateProcess(const char *path, const char *name = 0,
		int priority = PM::DEF_PRIORITY, const char *args = 0);

	virtual GEvent *CreateEvent();

	DECLARE_GCLASS_IMP(GApp);
};

#endif /* APP_H_ */
