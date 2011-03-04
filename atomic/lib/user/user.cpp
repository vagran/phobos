/*
 * /phobos/lib/user/user.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

/* User space runtime support library */

#include <sys.h>
phbSource("$Id$");

ULib *uLib;

void
__assert(const char *file, u32 line, const char *cond)
{
	CString s;
	s.Format("Assert failed at '%s@%lu': '%s'", file, line, cond);
	uLib->GetApp()->Abort(s.GetBuffer());
}

ULib::ULib(GApp *app) : alloc(app)
{
	app->AddRef();
	this->app = app;
	this->proc = app->GetProcess();
	vfs = app->GetVFS();
	time = app->GetTime();
	sOut = app->GetStream("output");
	sIn = app->GetStream("input");
	sTrace = app->GetStream("trace");
	sLog = app->GetStream("log");
}

ULib::~ULib()
{
	sOut->Release();
	sIn->Release();
	sTrace->Release();
	sLog->Release();
	time->Release();
	vfs->Release();
	proc->Release();
	app->Release();
}

ULib *
ULib::Initialize(GApp *app)
{
	assert(!uLib);
	uLib = (ULib *)app->AllocateHeap(sizeof(ULib));
	ensure(uLib);
	construct(uLib, ULib, app);

	return uLib;
}

int
ULib::StreamPrintfV(GStream *s, const char *fmt, va_list args)
{
	CString str;
	str.FormatV(fmt, args);
	char *buf = str.GetBuffer();
	int len = str.GetLength();
	while (len) {
		int rc = s->Write((u8 *)buf, len);
		if (rc < 0) {
			return rc;
		}
		if (!rc) {
			return -1;
		}
		len -= rc;
	}
	return 0;
}

int
ULib::StreamPrintf(GStream *s, const char *fmt,...)
{
	va_list va;
	va_start(va, fmt);
	return StreamPrintfV(s, fmt, va);
}

void
ULib::ThreadEntry(ThreadParams *params)
{
	GThread::ThreadFunc func = params->func;
	void *arg = params->arg;
	::mfree(params);
	uLib->GetApp()->ExitThread(func(arg));
	/* NOT REACHED */
}

GThread *
ULib::CreateThread(GProcess *proc, GThread::ThreadFunc func, void *arg,
		u32 stackSize, u32 priority)
{
	ThreadParams *params = ALLOC(ThreadParams, 1);
	if (!params) {
		return 0;
	}
	params->func = func;
	params->arg = arg;
	GThread *thrd = proc->CreateThread((GThread::ThreadFunc)ThreadEntry, params,
		stackSize, priority);
	if (!thrd) {
		mfree(params);
	}
	return thrd;
}
