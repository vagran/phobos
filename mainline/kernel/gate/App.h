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

#include <gate/GProcess.h>
#include <gate/GateStream.h>

DECLARE_GCLASS(GApp);

class GApp : public GateObject {
private:
	GProcess *gproc;
public:
	GApp();
	virtual ~GApp();

	virtual void ExitThread(int exitCode = 0) __noreturn;

	virtual GProcess *GetProcess();
	virtual GStream *GetStream(char *name);

	DECLARE_GCLASS_IMP(GApp);
};

#endif /* APP_H_ */
