/*
 * /phobos/lib/startup/AppStartup.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

extern int Main(GApp *app);

extern "C" void AppStart(GApp *app);

void
AppStart(GApp *app)
{
	app->ExitThread(Main(app));
	/* NOT REACHED */
}
