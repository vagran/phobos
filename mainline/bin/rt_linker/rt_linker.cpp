/*
 * /phobos/bin/rt_linker/rt_linker.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

int
Main(GApp *app)
{
	GProcess *proc = app->GetProcess();
	CString str(GETSTR(proc, GProcess::GetName));
	str += ' ';
	str += GETSTR(proc, GProcess::GetArgs);
	str += '\n';
	GStream *out = app->GetStream("output");
	out->Write((u8 *)str.GetBuffer(), str.GetLength());

	proc->SetArgs("myarg1 myarg2 3 4 5 6");

	str = GETSTR(proc, GProcess::GetArgs);
	str += '\n';
	out->Write((u8 *)str.GetBuffer(), str.GetLength());

	return 0;
}
