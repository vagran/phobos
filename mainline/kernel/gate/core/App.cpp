/*
 * /phobos/kernel/gate/App.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

DEFINE_GCLASS(GApp);

GApp::GApp()
{
	gproc = GNEW(gateArea, GProcess);
	gtime = GNEW(gateArea, GTime);
}

GApp::~GApp()
{
	gtime->KRelease();
	gproc->KRelease();
}

void
GApp::ExitThread(int exitCode)
{
	PM::Thread::GetCurrent()->Exit(exitCode);
}

int
GApp::Sleep(u64 time)
{
	return pm->Sleep(PM::Thread::GetCurrent(), "GApp::Sleep", time);
}

GStream *
GApp::GetStream(const char *name)
{
	if (proc->CheckUserString(name)) {
		return 0;
	}
	GStream *stream = proc->GetStream(name);
	if (stream) {
		stream->AddRef();
	}
	return stream;
}

GProcess *
GApp::GetProcess()
{
	gproc->AddRef();
	return gproc;
}

GTime *
GApp::GetTime()
{
	gtime->AddRef();
	return gtime;
}
