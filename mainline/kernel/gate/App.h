/*
 * /phobos/kernel/gate/App.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef APP_H_
#define APP_H_
#include <sys.h>
phbSource("$Id$");

DECLARE_GCLASS(GApp);

class GApp : public GateObject {
private:

public:
	GApp();
	virtual ~GApp();

	virtual PM::pid_t GetPID();

	DECLARE_GCLASS_IMP(GApp);
};

#endif /* APP_H_ */
