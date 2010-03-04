/*
 * /phobos/kernel/sys/elf.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#ifndef ELF_H_
#define ELF_H_
#include <sys.h>
phbSource("$Id$");

#include <elf_common.h>
#include <elf32.h>

typedef Elf32_Ehdr ElfEhdr;
typedef Elf32_Phdr ElfPhdr;
typedef Elf32_Shdr ElfShdr;

class ElfImageLoader : public PM::ImageLoader {
private:
	ElfEhdr ehdr;
	int status;
public:
	ElfImageLoader(VFS::File *file);
	virtual ~ElfImageLoader();

	DeclareILFactory();
	DeclareILProber();

	virtual int Load(MM::Map *map);
	virtual vaddr_t GetEntryPoint();
};

#endif /* ELF_H_ */
