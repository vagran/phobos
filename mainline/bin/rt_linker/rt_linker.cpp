/*
 * /phobos/bin/rt_linker/rt_linker.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include "rt_linker.h"

int
Main(GApp *app)
{
	RTLinker lkr;
	return lkr.Link();
}

RTLinker::RTLinker()
{
	target = GETSTR(uLib->GetProcess(), GProcess::GetArgs);
	int len = target.Find(' ');
	if (len != -1) {
		target.Truncate(len);
	}
}

RTLinker::~RTLinker()
{

}

int
RTLinker::Link()
{
	printf("Target: %s\n", target.GetBuffer());

	GFile *fd = uLib->GetVFS()->CreateFile(target.GetBuffer());
	if (!fd) {
		printf("Failed to open file\n");
		return -1;
	}

	if (elf_version (EV_CURRENT ) == EV_NONE) {
	     printf("ELF library initialization failed: %s" , elf_errmsg(-1));
	     return -1;
	}

	Elf *elf;

	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL){
	         printf("elf_begin() failed\n");
	         return -1;
	}

	size_t shstrndx;
	if (elf_getshdrstrndx(elf, &shstrndx)) {
		printf("elf_getshdrstrndx () failed : %s\n", elf_errmsg(-1));
		return -1;
	}

	Elf_Scn* scn = 0;
	while ((scn = elf_nextscn(elf, scn)) != 0) {
		Elf32_Shdr *shdr;
		char *name;
		if ((shdr = elf32_getshdr(scn)) != 0) {
			if (!(name = elf_strptr(elf, shstrndx, shdr->sh_name))) {
				printf(" elf_strptr () failed : %s\n", elf_errmsg(-1));
				return -1;
			}
			printf("Section %ld %s\n", (u32)elf_ndxscn(scn), name);
		}
	}

	return 0;
}
