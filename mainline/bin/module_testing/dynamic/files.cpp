/*
 * /phobos/bin/module_testing/dynamic/files.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <module_test.h>

class MTFiles : public MTest {
private:
public:
	MT_DECLARE(MTFiles);
};

MT_DESTR(MTFiles) {}

MT_DEFINE(MTFiles, "Filesystem operations")
{
	//GApp *app = uLib->GetApp();
	GFile *file = uLib->GetVFS()->CreateFile("/etc/test.txt",
		GVFS::CF_READ | GVFS::CF_WRITE | GVFS::CF_EXISTING | GVFS::CF_EXCLUSIVE);
	mt_assert(file);

#define PATTERN		"0123456789abcdef"
#define PATSIZE		(sizeof(PATTERN) - 1)
#define FILESIZE	263

	off_t offs;
	mt_assert(file->GetSize(&offs) == FILESIZE);
	mt_assert(offs == FILESIZE);

	CString s;
	mt_assert(file->Read(s.LockBuffer(PATSIZE), PATSIZE) == PATSIZE);
	s.ReleaseBuffer(PATSIZE);
	mt_assert(s == PATTERN);
	mt_assert(!file->Eof());

#define OFFS1	8
	mt_assert(file->Seek(OFFS1, GFile::SF_SET, &offs) == OFFS1);
	mt_assert(offs == OFFS1);
	u32 len = PATSIZE - OFFS1;
	mt_assert(file->Read(s.LockBuffer(len), len) == len);
	s.ReleaseBuffer(len);
	mt_assert(s == &PATTERN[OFFS1]);

	mt_assert(file->Seek(-(off_t)PATSIZE, GFile::SF_END, &offs) == FILESIZE - PATSIZE);
	mt_assert(offs == FILESIZE - PATSIZE);
	mt_assert(!file->Eof());
	mt_assert(file->Read(s.LockBuffer(10000), 10000) == PATSIZE);
	s.ReleaseBuffer(PATSIZE);
	mt_assert(file->Eof());
	mt_assert(s == PATTERN);

	file->Release();
}
