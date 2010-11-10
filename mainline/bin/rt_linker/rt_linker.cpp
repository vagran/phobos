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

#	define PRINTSTR()	out->Write((u8 *)str.GetBuffer(), str.GetLength())

	GProcess *proc = app->GetProcess();
	CString str(GETSTR(proc, GProcess::GetName));
	str += ' ';
	str += GETSTR(proc, GProcess::GetArgs);
	str += '\n';
	GStream *out = app->GetStream("output");
	PRINTSTR();

	proc->SetArgs("myarg1 myarg2 3 4 5 6");

	str = GETSTR(proc, GProcess::GetArgs);
	str += '\n';
	PRINTSTR();

	GVFS *vfs = app->GetVFS();
	GFile *file = vfs->CreateFile("/etc/test.txt");
	u32 sz = file->Read(str.LockBuffer(17), 16);
	str.ReleaseBuffer(16);
	str.Format("%ld bytes: '%s'\n", sz, str.GetBuffer());
	PRINTSTR();
	return 0;
}
