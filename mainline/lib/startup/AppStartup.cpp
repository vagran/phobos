/*
 * /phobos/lib/startup/AppStartup.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

extern int Main(void *arg);

extern "C" int AppStart(void *arg);

int
AppStart(void *arg)
{
	int rc = Main(arg);
	return rc;
}
