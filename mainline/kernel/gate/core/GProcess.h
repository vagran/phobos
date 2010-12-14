/*
 * /phobos/kernel/gate/GProcess.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#ifndef GPROCESS_H_
#define GPROCESS_H_
#include <sys.h>
phbSource("$Id$");

DECLARE_GCLASS(GProcess);

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

	/* Wait on state change, return immediately if already terminated */
	virtual PM::waitid_t GetWaitChannel(Operation op);
	virtual int OpAvailable(Operation op);

	DECLARE_GCLASS_IMP(GProcess);
};

#endif /* GPROCESS_H_ */
