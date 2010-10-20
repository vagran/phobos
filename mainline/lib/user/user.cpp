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
}

ULib::~ULib()
{
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
