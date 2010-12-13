/*
 * /phobos/lib/startup/AppStartup.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

extern "C" void *__dso_handle;
void *__dso_handle;

extern int Main(GApp *app);

extern "C" void AppStart(GApp *app);

void
AppStart(GApp *app)
{
	/* Initialize PhobOS user space run-time support library */
	ULib::Initialize(app);
	/* call global constructors */
	CXA::ConstructStaticObjects();
	/* call Main() function */
	app->ExitThread(Main(app));
	/* NOT REACHED */
}

RTLinker::DSOHandle
GetDSO(RTLinker **linker)
{
	if (__dso_handle) {
		if (linker) {
			*linker = ((RTLinker::DynObject *)__dso_handle)->linker;
		}
		return (RTLinker::DSOHandle)__dso_handle;
	}
	return 0;
}
