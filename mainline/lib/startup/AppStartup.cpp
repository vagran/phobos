/*
 * /phobos/lib/startup/AppStartup.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

extern int Main(GApp *app);

extern "C" void AppStart(GApp *app);

void
AppStart(GApp *app)
{
	/* zero BSS */
	memset(&_edata, 0, (u32)&_end - (u32)&_edata);
	/* call global constructors */
	CXA::ConstructStaticObjects();
	/* XXX should dynamically load libraries */
	ULib::Initialize(app);
	/* call Main() function */
	app->ExitThread(Main(app));
	/* NOT REACHED */
}
