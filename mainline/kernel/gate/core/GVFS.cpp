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

void *
GFile::Map(u32 len, u32 flags, u32 offset, void *location)
{
	if (!len) {
		len = roundup2(size, PAGE_SIZE);
	} else {
		len = roundup2(len, PAGE_SIZE);
	}
	if (offset & (PAGE_SIZE - 1)) {
		ERROR(E_INVAL, "Offset should be multiple of PAGE_SIZE (0x%lx)", offset);
		return 0;
	}
	if (((vaddr_t)location) & (PAGE_SIZE - 1)) {
		ERROR(E_INVAL, "Location should be multiple of PAGE_SIZE (0x%lx)",
			(vaddr_t)location);
		return 0;
	}

	int prot = 0;
	if (flags & MF_PROT_READ) {
		prot |= MM::PROT_READ;
	}
	if (flags & MF_PROT_EXEC) {
		prot |= MM::PROT_EXEC;
	}
	if (flags & MF_PROT_WRITE) {
		if (flags & MF_SHARED) {
			prot |= MM::PROT_WRITE;
		} else {
			prot |= MM::PROT_COW;
		}
	}

	MM::VMObject *obj = vfs->MapFile(file);
	if (!obj) {
		ERROR(E_FAULT, "Cannot create VM object for file");
		return 0;
	}

	MM::Map::Entry *e = 0;
	if ((flags & MF_FIXED) || location) {
		e = proc->GetUserMap()->InsertObjectAt(obj, (vaddr_t)location, offset,
			len, prot);
		if (!e && (flags & MF_FIXED)) {
			obj->Release();
			ERROR(E_FAULT, "Cannot insert object to specified location "
				"(%ld bytes at 0x%lx)", len, (vaddr_t)location);
			return 0;
		}
	}
	if (!e) {
		e = proc->GetUserMap()->InsertObject(obj, offset, len, prot);
		if (!e) {
			obj->Release();
			ERROR(E_FAULT, "Cannot insert object to process map");
			return 0;
		}
	}
	obj->Release();
	return (void *)e->base;
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
	gfile->AddRef();
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

int
GVFS::UnMap(void *map)
{
	return proc->GetUserMap()->Free((vaddr_t)map);
}
