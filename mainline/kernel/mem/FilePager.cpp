/*
 * /phobos/kernel/mem/FilePager.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <mem/FilePager.h>

FilePager::FilePager(vsize_t size, Handle handle) :
	MM::Pager(T_FILE, size, handle)
{
	file = (VFS::File *)handle;
	file->AddRef();
}

FilePager::~FilePager()
{
	file->Release();
}

int
FilePager::HasPage(vaddr_t offset)
{
	//notimpl
	return 1;
}

int
FilePager::GetPage(vaddr_t offset, MM::Page **ppg, int numPages)
{
	MM::Map::Entry *buf = MapPages(ppg, numPages);
	if (!buf) {
		return -1;
	}
	u32 size = numPages * PAGE_SIZE;
	int rc = file->Read(offset, (void *)buf->base, size) == size ? 0 : -1;
	UnmapPages(buf);
	return rc;
}

int
FilePager::PutPage(vaddr_t offset, MM::Page **ppg, int numPages)
{
	//notimpl
	return 0;
}
