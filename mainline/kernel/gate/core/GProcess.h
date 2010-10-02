/*
 * /phobos/kernel/gate/GProcess.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
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
	virtual int GetName(char *buf, int bufLen);

	DECLARE_GCLASS_IMP(GProcess);
};

#endif /* GPROCESS_H_ */
