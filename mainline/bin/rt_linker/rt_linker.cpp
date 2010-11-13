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
	str = "Path: ";
	str += GETSTR(file, GFile::GetPath);
	str += '\n';
	PRINTSTR();

	CString str1;
	u32 sz = file->Read(str1.LockBuffer(17), 16);
	str1.ReleaseBuffer(sz);
	str.Format("%ld bytes: '%s'\n", sz, str1.GetBuffer());
	PRINTSTR();
	sz = file->Read(str1.LockBuffer(3000), 3000);
	str1.ReleaseBuffer(sz);
	str.Format("%ld bytes: '%s'\n", sz, str1.GetBuffer());
	PRINTSTR();

	file->Seek(6);
	sz = file->Read(str1.LockBuffer(3000), 3000);
	str1.ReleaseBuffer(sz);
	str.Format("%ld bytes: '%s'\n", sz, str1.GetBuffer());
	PRINTSTR();

	file->Seek(-50, GFile::SF_END);
	sz = file->Read(str1.LockBuffer(3000), 3000);
	str1.ReleaseBuffer(sz);
	str.Format("%ld bytes: '%s'\n", sz, str1.GetBuffer());
	PRINTSTR();
	return 0;
}
