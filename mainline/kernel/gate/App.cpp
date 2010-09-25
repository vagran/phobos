/*
 * /phobos/kernel/gate/App.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

DEFINE_GCLASS(GApp);

GApp::GApp()
{
	gproc = GNEW(gateArea, GProcess);
}

GApp::~GApp()
{

}

void
GApp::ExitThread(int exitCode)
{
	PM::Thread::GetCurrent()->Exit(exitCode);
}

GStream *
GApp::GetStream(char *name)
{
	//notimpl
	return 0;
}

GProcess *
GApp::GetProcess()
{
	return gproc;
}
