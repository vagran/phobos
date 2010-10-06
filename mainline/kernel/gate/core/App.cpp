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

void
GApp::Abort(char *msg)
{
	PM::Thread *thrd = PM::Thread::GetCurrent();
	if (msg && !proc->CheckUserString(msg)) {
		thrd->Fault(PM::PFLT_ABORT, "%s", msg);
	} else {
		thrd->Fault(PM::PFLT_ABORT);
	}
}

int
GApp::Sleep(u64 time)
{
	return pm->Sleep(PM::Thread::GetCurrent(), "GApp::Sleep", time);
}

int
GApp::Wait()
{
	//notimpl
	ERROR(E_FAULT, "Test error here %x", 237);//temp
	ERROR(E_INVAL);
	ERROR(E_NOMEM, "Another test error here %d", 201);//temp
	return 0;
}

int
GApp::GetLastErrorStr(char *buf, int bufLen)
{
	/* get error object for previous system call */
	Error *e = PM::Thread::GetCurrent()->GetError(1);
	/* return to previous error object in order to not change history by this call */
	PM::Thread::GetCurrent()->PrevError();
	return e ? e->GetStr(buf, bufLen) : KString("No errors reported\n").Get(buf, bufLen);
}

Error::ErrorCode
GApp::GetLastError()
{
	/* get error object for previous system call */
	Error *e = PM::Thread::GetCurrent()->GetError(1);
	/* return to previous error object in order to not change history by this call */
	PM::Thread::GetCurrent()->PrevError();
	return e ? e->GetCode() : Error::E_SUCCESS;
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
