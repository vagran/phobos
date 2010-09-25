/*
 * /phobos/bin/init/init.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

int myGlobalBSS, myGlobalInit = 237;

int
Main(GApp *app)
{
	GProcess *proc = app->GetProcess();
	CString str(GETSTR(proc, GProcess::GetName));
	str += ' ';
	str += GETSTR(proc, GProcess::GetName);
	str.GetBuffer();
	return proc->GetPID();//temp
}
