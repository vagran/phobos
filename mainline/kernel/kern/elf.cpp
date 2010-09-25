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
	if (!IS_ELF(&ehdr)) {
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
	ElfEhdr *hdr = ALLOC(ElfEhdr, 1);
	int rc = -1;

	do {
		if (file->Read(0, hdr, sizeof(*hdr)) != sizeof(*hdr)) {
			break;
		}
		if (!IS_ELF(hdr)) {
			break;
		}
		if (hdr->e_machine != EM_386) {
			break;
		}
		if (hdr->e_ident[EI_CLASS] != ELFCLASS32) {
			break;
		}
		if (hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
			break;
		}
		rc = 0;
	} while (0);
	FREE(hdr);
	return rc;
}

int
ElfImageLoader::Load(MM::Map *map, MM::VMObject *bssObj)
{
	if (status) {
		return E_FAULT;
	}
	ElfPhdr *phdr = ALLOC(ElfPhdr, 1);
	if (!phdr) {
		return E_NOMEM;
	}
	MM::VMObject *obj = vfs->MapFile(file);
	if (!obj) {
		return E_FAULT;
	}
	/* iterate through program header entries */
	for (int i = 0; i < ehdr.e_phnum; i++) {
		if (file->Read(ehdr.e_phoff + ehdr.e_phentsize * i, phdr,
			sizeof(*phdr)) != sizeof(*phdr)) {
			obj->Release();
			FREE(phdr);
			return E_IO;
		}
		/* load only segments of PT_LOAD type */
		if (phdr->p_type != PT_LOAD) {
			continue;
		}
		u64 start_off = phdr->p_offset;
		vaddr_t start_va = phdr->p_vaddr;
		u32 file_size = phdr->p_filesz;
		vsize_t mem_size = phdr->p_memsz;
		if (phdr->p_align != 0 && phdr->p_align != 1 && ispowerof2(phdr->p_align)) {
			u32 pad = start_off - rounddown2(start_off, phdr->p_align);
			file_size += pad;
			start_off -= pad;
			pad = start_va - rounddown2(start_va, phdr->p_align);
			mem_size += pad;
			start_va -= pad;
		}
		if ((start_va & (PAGE_SIZE - 1)) || (start_off & (PAGE_SIZE - 1))) {
			klog(KLOG_ERROR, "Unaligned executable binary segments loading "
				"is not supported");
			obj->Release();
			FREE(phdr);
			return E_INVAL;
		}
		/* build protection value */
		int protection = 0;
		if (phdr->p_flags & PF_R) {
			protection |= MM::PROT_READ;
		}
		if (phdr->p_flags & PF_W) {
			protection |= MM::PROT_COW;
		}
		if (phdr->p_flags & PF_X) {
			protection |= MM::PROT_EXEC;
		}
		if (!map->InsertObjectAt(obj, start_va, start_off, file_size,
			protection)) {
			obj->Release();
			FREE(phdr);
			return E_FAULT;
		}
		/* Insert BSS chunk if required */
		if (mem_size > roundup2(file_size, PAGE_SIZE)) {
			protection &= ~MM::PROT_COW;
			if (phdr->p_flags & PF_W) {
				protection |= MM::PROT_WRITE;
			}
			vaddr_t base = start_va + roundup2(file_size, PAGE_SIZE);
			if (!map->InsertObjectAt(bssObj, base, base,
					mem_size - roundup2(file_size, PAGE_SIZE), protection)) {
				obj->Release();
				FREE(phdr);
				return E_FAULT;
			}
		}
	}
	obj->Release();
	FREE(phdr);
	return 0;
}

vaddr_t
ElfImageLoader::GetEntryPoint()
{
	return ehdr.e_entry;
}
