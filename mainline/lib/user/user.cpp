/*
 * /phobos/lib/user/user.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
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
