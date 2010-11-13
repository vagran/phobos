/*
 * /phobos/kernel/gate/GProcess.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#ifndef GPROCESS_H_
#define GPROCESS_H_
#include <sys.h>
phbSource("$Id$");

DECLARE_GCLASS(GProcess);

DECLARE_GCLASS(GProcess);

class GProcess : public GateObject {
private:

public:
	GProcess();
	virtual ~GProcess();

	virtual PM::pid_t GetPID();
	virtual PM::pid_t GetThreadID();
	virtual DECL_STR_PROV(GetName);
	virtual DECL_STR_PROV(GetArgs);
	virtual int SetArgs(const char *args);
	virtual DECL_STR_PROV(GetEnv); /* arg - variable name */
	virtual int SetEnv(const char *name, char *value); /* value = 0 for deleting */

	DECLARE_GCLASS_IMP(GProcess);
};

#endif /* GPROCESS_H_ */
