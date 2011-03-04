/*
 * /phobos/kernel/mem/FilePager.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
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
	u32 len = file->Read(offset, (void *)buf->base, size);
	if (len < size) {
		/* zero remained space */
		memset((u8 *)buf->base + len, 0, size - len);
	}
	UnmapPages(buf);
	return 0;
}

int
FilePager::PutPage(vaddr_t offset, MM::Page **ppg, int numPages)
{
	//notimpl
	return 0;
}
