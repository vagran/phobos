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

}

ElfImageLoader::~ElfImageLoader()
{

}

DefineILFactory(ElfImageLoader);

DefineILProber(ElfImageLoader)
{
	//notimpl
	return 0;
}

int
ElfImageLoader::Load(MM::Map *map)
{
	//notimpl
	return 0;
}

vaddr_t
ElfImageLoader::GetEntryPoint()
{
	//notimpl
	return 0;
}
