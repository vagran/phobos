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
	file->AddRef();
	this->file = file;
	curPos = 0;
	size = file->GetSize();
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

u32
GFile::GetSize(off_t *pSize)
{
	if (pSize) {
		if (proc->CheckUserBuf(pSize, sizeof(*pSize), MM::PROT_WRITE)) {
			return -1;
		}
		*pSize = size;
	}
	return (u32)size;
}

int
GFile::Eof()
{
	return curPos == size;
}

int
GFile::Rename(const char *name)
{
	//notimpl
	return -1;
}

DEF_STR_PROV(GFile::GetName)
{
	if (buf && proc->CheckUserBuf(buf, bufLen, MM::PROT_WRITE)) {
		return -1;
	}
	KString str;
	file->GetName(str);
	return str.Get(buf, bufLen);
}

DEF_STR_PROV(GFile::GetPath)
{
	if (buf && proc->CheckUserBuf(buf, bufLen, MM::PROT_WRITE)) {
		return -1;
	}
	KString str;
	file->GetPath(str);
	return str.Get(buf, bufLen);
}

u32
GFile::Read(void *buf, u32 len)
{
	if (proc->CheckUserBuf(buf, len, MM::PROT_WRITE)) {
		return -1;
	}
	u32 sz = file->Read(curPos, buf, len);
	curPos += sz;
	return sz;
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
	off_t newPos;

	switch (flags & SF_WHENCE_MASK) {
	case SF_SET:
		newPos = offset;
		break;
	case SF_CUR:
		newPos = curPos + offset;
		break;
	case SF_END:
		newPos = size + offset;
		break;
	default:
		ERROR(E_INVAL, "Invalid whence specified: %ld", flags & SF_WHENCE_MASK);
		return -1;
	}
	if (newPos > size) {
		curPos = size;
	} else {
		curPos = newPos;
	}
	return curPos;
}

u32
GFile::GetCurPos(off_t *pos)
{
	if (pos) {
		if (proc->CheckUserBuf(pos, sizeof(*pos), MM::PROT_WRITE)) {
			return -1;
		}
		*pos = curPos;
	}
	return (u32)pos;
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
GVFS::CreateFile(const char *path, u32 flags, VFS::Node::Type type)
{
	if (proc->CheckUserString(path)) {
		return 0;
	}
	int vfsFlags = 0;
	if (flags & CF_EXISTING) {
		vfsFlags |= VFS::CFF_EXISTING;
	}
	// XXX the rest flags should be implemented
	VFS::File *file = vfs->CreateFile(path, vfsFlags, type);
	if (!file) {
		return 0;
	}
	GFile *gfile = GNEW(gateArea, GFile, file);
	file->Release();
	if (!gfile) {
		ERROR(E_FAULT, "Cannot create GFile object");
	}
	return gfile;
}

int
GVFS::DeleteFile(const char *path)
{
	if (proc->CheckUserString(path)) {
		return 0;
	}

	return 0;//notimpl
}
