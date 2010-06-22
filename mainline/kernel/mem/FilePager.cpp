/*
 * /phobos/kernel/mem/FilePager.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
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
	return 0;
}

int
FilePager::GetPage(MM::Page *pg, vaddr_t offset)
{
	//notimpl
	return 0;
}

int
FilePager::PutPage(MM::Page *pg, vaddr_t offset)
{
	//notimpl
	return 0;
}
