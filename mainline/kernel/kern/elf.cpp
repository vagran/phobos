/*
 * /phobos/kernel/kern/elf.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <elf.h>

RegisterImageLoader(ElfImageLoader, "Executable and Linkable Format (ELF)");

ElfImageLoader::ElfImageLoader(VFS::File *file) : PM::ImageLoader(file)
{
	status = -1;
	if (file->Read(0, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
		return;
	}
	if (!IS_ELF(ehdr)) {
		return;
	}
	status = 0;
}

ElfImageLoader::~ElfImageLoader()
{
}

DefineILFactory(ElfImageLoader);

DefineILProber(ElfImageLoader)
{
	ElfEhdr hdr;

	if (file->Read(0, &hdr, sizeof(hdr)) != sizeof(hdr)) {
		return -1;
	}
	return IS_ELF(hdr) ? 0 : -1;
}

int
ElfImageLoader::Load(MM::Map *map)
{
	if (status) {
		return -1;
	}
	//notimpl
	return 0;
}

vaddr_t
ElfImageLoader::GetEntryPoint()
{
	return ehdr.e_entry;
}
