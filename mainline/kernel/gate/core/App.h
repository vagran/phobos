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
	GProcess *gproc;
	GTime *gtime;
public:
	GApp();
	virtual ~GApp();

	virtual void ExitThread(int exitCode = 0) __noreturn;
	virtual int Sleep(u64 time);

	virtual GProcess *GetProcess();
	virtual GStream *GetStream(const char *name);
	virtual GTime *GetTime();

	DECLARE_GCLASS_IMP(GApp);
};

#endif /* APP_H_ */
