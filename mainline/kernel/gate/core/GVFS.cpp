/*
 * /phobos/kernel/gate/gvfs.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

/* GFile class */

DEFINE_GCLASS(GFile);

GFile::GFile(VFS::File *file)
{
	this->file = file;
}

GFile::~GFile()
{
	if (file) {
		file->Release();
	}
}

int
GFile::Truncate()
{
	//notimpl
	return -1;
}

int
GFile::GetSize(off_t *pSize)
{
	//notimpl
	return -1;
}

int
GFile::Rename(const char *name)
{
	//notimpl
	return -1;
}

DEF_STR_PROV(GFile::GetName)
{
	//notimpl
	return -1;
}

DEF_STR_PROV(GFile::GetPath)
{
	//notimpl
	return -1;
}

u32
GFile::Read(void *buf, u32 len)
{
	//notimpl
	return 0;
}

u32
GFile::Write(void *buf, u32 len)
{
	//notimpl
	return 0;
}

u32
GFile::Seek(off_t offset, u32 flags, off_t *newOffset)
{
	//notimpl
	return 0;
}

u32
GFile::GetCurPos(off_t *pos)
{
	//notimpl
	return 0;
}

VFS::Node::Type
GFile::GetType()
{
	return file->GetType();
}

/* GVFS class */

DEFINE_GCLASS(GVFS);

GVFS::GVFS()
{

}

GVFS::~GVFS()
{

}

GFile *
GVFS::CreateFile(const char *path, u32 flags)
{
	if (proc->CheckUserString(path)) {
		return 0;
	}

	return 0;//notimpl
}

int
GVFS::DeleteFile(const char *path)
{
	if (proc->CheckUserString(path)) {
		return 0;
	}

	return 0;//notimpl
}
