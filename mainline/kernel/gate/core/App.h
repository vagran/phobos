/*
 * /phobos/kernel/gate/App.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#ifndef APP_H_
#define APP_H_
#include <sys.h>
phbSource("$Id$");

#include <gate/core/GProcess.h>
#include <gate/io/GateStream.h>
#include <gate/core/GTime.h>

DECLARE_GCLASS(GApp);

class GApp : public GateObject {
private:
	enum {
		MAX_WAIT_OBJECTS =		1024, /* Maximal number of object for Wait() method */
	};

	GProcess *gproc;
	GTime *gtime;
public:
	GApp();
	virtual ~GApp();

	virtual void ExitThread(int exitCode = 0) __noreturn;
	virtual void Abort(char *msg = 0);
	virtual int Sleep(u64 time);
	virtual int Wait(Operation op, GateObject **objects, int numObjects,
		void *bitmap = 0, u64 timeout = 0);

	virtual int GetLastErrorStr(char *buf, int bufLen);
	virtual Error::ErrorCode GetLastError();

	virtual GProcess *GetProcess();
	virtual GStream *GetStream(const char *name);
	virtual GTime *GetTime();

	virtual void *AllocateHeap(u32 size, int prot = MM::PROT_READ | MM::PROT_WRITE);
	virtual u32 GetHeapSize(void *p);
	virtual int FreeHeap(void *p);

	DECLARE_GCLASS_IMP(GApp);
};

#endif /* APP_H_ */
