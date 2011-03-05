/*
 * /phobos/kernel/gate/GProcess.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#ifndef GPROCESS_H_
#define GPROCESS_H_
#include <sys.h>
phbSource("$Id$");

DECLARE_GCLASS(GThread);

class GThread : public GateObject {
public:
	typedef int (*ThreadFunc)(void *arg);
private:
	PM::Thread *refThrd;
	PM::Thread::State lastState; /* Used for wait on thread */
public:
	GThread(PM::Thread *refThrd = 0);
	virtual ~GThread();

	virtual PM::pid_t GetPID();
	virtual PM::Thread::State GetState(u32 *pExitCode = 0, PM::ProcessFault *pFault = 0);
	/* Wait on state change, return immediately if already terminated */
	virtual waitid_t GetWaitChannel(Operation op);
	virtual int OpAvailable(Operation op);

	DECLARE_GCLASS_IMP(GThread);
};

DECLARE_GCLASS(GProcess);

class GProcess : public GateObject {
private:
	PM::Process *refProc;
	PM::Process::State lastState; /* Used for wait on process */
public:
	GProcess(PM::Process *refProc = 0);
	virtual ~GProcess();

	virtual PM::pid_t GetPID();
	virtual DECL_STR_PROV(GetName);
	virtual DECL_STR_PROV(GetArgs);
	virtual int SetArgs(const char *args);
	/* arg - variable name, dump entire environment if zero */
	virtual DECL_STR_PROV(GetEnv);
	virtual int SetEnv(const char *name, char *value); /* value = 0 for deleting */
	virtual PM::Process::State GetState(u32 *pExitCode = 0, PM::ProcessFault *pFault = 0);
	virtual GThread *CreateThread(GThread::ThreadFunc func, void *arg = 0,
		u32 stackSize = PM::Thread::DEF_STACK_SIZE, u32 priority = PM::DEF_PRIORITY);

	/* Wait on state change, return immediately if already terminated */
	virtual waitid_t GetWaitChannel(Operation op);
	virtual int OpAvailable(Operation op);

	DECLARE_GCLASS_IMP(GProcess);
};

#endif /* GPROCESS_H_ */
