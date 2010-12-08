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

typedef int (*EntryFunc)(GApp *app);

int
Main(GApp *app)
{
	RTLinker lkr;
	int rc = lkr.Link();
	if (rc) {
		return rc;
	}
	EntryFunc ef = (EntryFunc)lkr.GetEntry();
	return ef(app);
}

RTLinker::RTLinker()
{
	dynTree = 0;
}

RTLinker::~RTLinker()
{
	DestroyObjTree();
}

void
RTLinker::ErrorV(const char *msg, va_list args)
{
	printf("Run-time linker: ");
	vprintf(msg, args);
	printf("\n");
}

void
RTLinker::Error(const char *msg, ...)
{
	va_list va;
	va_start(va, msg);
	ErrorV(msg, va);
}

Elf_Scn *
RTLinker::FindSection(Elf *elf, const char *name)
{
	size_t shstrndx;
	if (elf_getshdrstrndx(elf, &shstrndx)) {
		Error("elf_getshdrstrndx () failed : %s\n", elf_errmsg());
		return 0;
	}

	Elf_Scn *scn = 0;
	while ((scn = elf_nextscn(elf, scn)) != 0) {
		Elf32_Shdr *shdr;
		char *sectName;
		if ((shdr = elf32_getshdr(scn)) != 0) {
			if ((sectName = elf_strptr(elf, shstrndx, shdr->sh_name))) {
				if (!strcmp(sectName, name)) {
					return scn;
				}
			}
		}
	}
	return 0;
}

Elf_Scn *
RTLinker::FindSection(Elf *elf, Elf32_Word type)
{
	Elf_Scn *scn = 0;
	while ((scn = elf_nextscn(elf, scn)) != 0) {
		Elf32_Shdr *shdr;
		if ((shdr = elf32_getshdr(scn)) != 0) {
			if (shdr->sh_type == type) {
				return scn;
			}
		}
	}
	return 0;
}

Elf_Scn *
RTLinker::FindSectionByAddr(Elf *elf, Elf32_Word addr)
{
	Elf_Scn *scn = 0;
	while ((scn = elf_nextscn(elf, scn)) != 0) {
		Elf32_Shdr *shdr;
		if ((shdr = elf32_getshdr(scn)) != 0) {
			if (shdr->sh_addr == addr) {
				return scn;
			}
		}
	}
	return 0;
}

RTLinker::DynObject_s::DynObject_s(RTLinker *linker, DynObject *parent) :
	linkCtx(this)
{
	this->linker = linker;
	LIST_INIT(deps);
	LIST_INIT(segments);
	LIST_INIT(sections);
	numDeps = 0;
	flags = 0;
	this->parent = parent;
	if (parent) {
		LIST_ADD(depList, this, parent->deps);
		parent->numDeps++;
	}
}

RTLinker::DynObject_s::~DynObject_s()
{
	if (parent) {
		parent->numDeps--;
		LIST_DELETE(depList, this, parent->deps);
	}
	ObjSegment *seg;
	while ((seg = LIST_FIRST(ObjSegment, list, segments))) {
		LIST_DELETE(list, seg, segments);
		DELETE(seg);
	}
	ObjSection *scn;
	while ((scn = LIST_FIRST(ObjSection, list, sections))) {
		LIST_DELETE(list, scn, sections);
		DELETE(scn);
	}
}

RTLinker::ObjSegment *
RTLinker::DynObject_s::AddSegment()
{
	ObjSegment *seg = NEW(ObjSegment);
	if (!seg) {
		return 0;
	}
	memset(seg, 0, sizeof(*seg));
	LIST_ADD(list, seg, segments);
	return seg;
}

RTLinker::ObjSection *
RTLinker::DynObject_s::AddSection()
{
	ObjSection *scn = NEW(ObjSection);
	if (!scn) {
		return 0;
	}
	memset(scn, 0, sizeof(*scn));
	LIST_ADD(list, scn, sections);
	return scn;
}

RTLinker::ObjSection *
RTLinker::DynObject_s::FindSection(Elf32_Word type, ObjSection *prev)
{
	ObjSection *scn;
	if (prev) {
		if (LIST_ISLAST(list, prev, sections)) {
			return 0;
		}
		scn = LIST_NEXT(ObjSection, list, prev);
	} else {
		scn = LIST_FIRST(ObjSection, list, sections);
	}
	do {
		if (scn->type == type) {
			return scn;
		}
		if (LIST_ISLAST(list, scn, sections)) {
			break;
		}
		scn = LIST_NEXT(ObjSection, list, scn);
	} while (1);
	return 0;
}

RTLinker::ObjSection *
RTLinker::DynObject_s::FindSectionByAddr(vaddr_t addr)
{
	ObjSection *scn;
	LIST_FOREACH(ObjSection, list, scn, sections) {
		if (addr >= scn->baseAddr && addr < scn->baseAddr + scn->size) {
			return scn;
		}
	}
	return 0;
}

void
RTLinker::DynObject_s::ErrorV(const char *msg, va_list args)
{
	CString str;
	str.Format("Object '%s': ", path.GetBuffer());
	str.AppendFormatV(msg, args);
	linker->Error("%s", str.GetBuffer());
}

void
RTLinker::DynObject_s::Error(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	ErrorV(msg, args);
}

Elf32_Dyn *
RTLinker::FindDynTag(ObjContext *ctx, Elf32_Sword tag, Elf32_Dyn *prev)
{
	if (!ctx->dynScn) {
		return 0;
	}
	Elf_Data *data = 0;
	while (prev || (data = elf_getdata(ctx->dynScn, data))) {
		if (prev) {
			data = ctx->lastDTBlock;
		} else {
			ctx->lastDTBlock = data;
			if (data->d_type != ELF_T_DYN) {
				break;
			}
			if (!data->d_buf) {
				continue;
			}
		}
		Elf32_Dyn *dt;
		for (dt = prev ? prev + 1 : (Elf32_Dyn *)data->d_buf;
			dt < (Elf32_Dyn *)data->d_buf + data->d_size / sizeof(Elf32_Dyn);
			dt++) {
			if (dt->d_tag == tag) {
				return dt;
			}
		}
		if (prev) {
			prev = 0;
		}
	}
	return 0;
}

int
RTLinker::GetDepChain(DynObject *obj, CString &s)
{
	s.Empty();
	while (obj) {
		s += obj->path;
		obj = obj->parent;
		if (obj) {
			s += " <= ";
		}
	}
	return 0;
}

int
RTLinker::ProcessObjDeps(ObjContext *ctx)
{
	Elf32_Dyn *dt;
	if (!ctx->dynStrScn) {
		if (!(dt = FindDynTag(ctx, DT_STRTAB))) {
			Error("Cannot find dynamic strings table");
			return -1;
		}
		if (!(ctx->dynStrScn = FindSectionByAddr(ctx->elf, dt->d_un.d_val))) {
			Error("Cannot find dynamic strings table section");
			return -1;
		}
	}

	char *rpath = 0;
	if ((dt = FindDynTag(ctx, DT_RPATH))) {
		rpath = elf_strptr(ctx->elf, elf_ndxscn(ctx->dynStrScn), dt->d_un.d_val);
	}

	CString curObjName;
	GetDepChain(ctx->obj, curObjName);

	Elf32_Dyn *dep = 0;
	while ((dep = FindDynTag(ctx, DT_NEEDED, dep))) {
		char *name = elf_strptr(ctx->elf, elf_ndxscn(ctx->dynStrScn), dep->d_un.d_val);
		/*
		 * According to ELF specification:
		 * If a shared object name has one or more slash (/) characters anywhere in the name, such as
		 * /usr/lib/lib2 or directory/file, the dynamic linker uses that string directly as the path
		 * name. If the name has no slashes, such as lib1, three facilities specify shared object path
		 * searching.
		 *
		 */
		GFile *file = 0;
		if (strchr(name, '/')) {
			file = uLib->GetVFS()->CreateFile(name, GVFS::CF_READ);
		} else {
			CString path;

			if (rpath) {
				char *curPath = rpath;
				while (curPath && *curPath) {
					char *nextPath = strchr(curPath, ':');
					if (nextPath) {
						memcpy(path.LockBuffer(nextPath - curPath), curPath,
							nextPath - curPath);
						path.ReleaseBuffer(nextPath - curPath);
					} else {
						path = curPath;
					}
					if (path[path.GetLength() - 1] != '/') {
						path += '/';
					}
					path += name;
					if ((file = uLib->GetVFS()->CreateFile(path.GetBuffer(),
						GVFS::CF_READ))) {
						break;
					}
					curPath = nextPath;
					if (curPath) {
						curPath++;
					}
				}
			}
			if (!file) {
				path = __STR(RT_LINKER_DEFDIR);
				path += '/';
				path += name;
				file = uLib->GetVFS()->CreateFile(path.GetBuffer(), GVFS::CF_READ);
			}
		}

		if (!file) {
			Error("Cannot find dependency file: '%s', required by '%s'", name,
				curObjName.GetBuffer());
			return -1;
		}

		DynObject *obj = CreateObject(file, ctx->obj);
		file->Release();
		if (!obj) {
			Error("Cannot create object instance: '%s', required by '%s'", name,
				curObjName.GetBuffer());
			return -1;
		}
	}

	return 0;
}

RTLinker::DynObject *
RTLinker::GetNextObject(DynObject *prev)
{
	if (!prev) {
		return dynTree;
	}
	if (!LIST_ISEMPTY(prev->deps)) {
		return LIST_FIRST(DynObject, depList, prev->deps);
	}
	while (prev->parent) {
		if (!LIST_ISLAST(depList, prev, prev->parent->deps)) {
			return LIST_NEXT(DynObject, depList, prev);
		}
		prev = prev->parent;
	}
	return 0;
}

RTLinker::DynObject *
RTLinker::CreateObject(GFile *file, DynObject *parent)
{
	ObjContext ctx(file);

	/* Check for circular dependencies */
	CString path = GETSTR(file, GFile::GetPath);
	if (parent) {
		DynObject *root = parent;
		while (root->parent) {
			root = root->parent;
		}
		while (root) {
			if (root->path == path) {
				/* Duplicate object found */
				return root;
			}
			root = GetNextObject(root);
		}
	}

	if (!(ctx.elf = elf_begin(file, ELF_C_READ, NULL))) {
		Error("elf_begin() failed: %s", elf_errmsg());
		return 0;
	}

	if (elf_kind(ctx.elf) != ELF_K_ELF) {
		Error("Invalid type of ELF binary: %d", elf_kind(ctx.elf));
		return 0;
	}

	if (!(ctx.dynScn = FindSection(ctx.elf, SHT_DYNAMIC))) {
		Error("Cannot find dynamic section, not a dynamic executable");
		return 0;
	}

	if (!(ctx.obj = NEW(DynObject, this, parent))) {
		Error("Cannot allocate dynamic object instance");
		return 0;
	}
	ctx.obj->path = path;

	if (ProcessSegments(&ctx)) {
		Error("Filed to process segments");
		return 0;
	}

	if (ProcessSections(&ctx)) {
		Error("Filed to process sections");
		return 0;
	}

	if (ProcessObjDeps(&ctx)) {
		Error("Cannot process object dependencies");
		return 0;
	}

	return ctx.PopObject();
}

int
RTLinker::ProcessSegments(ObjContext *ctx)
{
	if (!ctx->ehdr) {
		if (!(ctx->ehdr = elf32_getehdr(ctx->elf))) {
			Error("Cannot get ELF header: %s", elf_errmsg());
			return -1;
		}
	}
	if (ctx->ehdr->e_type == ET_DYN) {
		ctx->obj->flags |= DynObject::F_RELOCATABLE;
	}
	if (!ctx->phdr) {
		if (!(ctx->phdr = elf32_getphdr(ctx->elf))) {
			Error("Cannot get program header: %s", elf_errmsg());
			return -1;
		}
	}
	ctx->obj->entry = ctx->ehdr->e_entry;

	/* iterate through program header entries */
	for (int i = 0; i < ctx->ehdr->e_phnum; i++) {
		Elf32_Phdr *phdr = (Elf32_Phdr *)((u8 *)ctx->phdr + ctx->ehdr->e_phentsize * i);
		/* Skip not loadable segments */
		if (phdr->p_type != PT_LOAD) {
			continue;
		}
		ObjSegment *seg = ctx->obj->AddSegment();
		if (!seg) {
			Error("Cannot allocate segment descriptor");
			return -1;
		}
		seg->fileOffset = phdr->p_offset;
		seg->baseAddr = phdr->p_vaddr;
		seg->fileSize = phdr->p_filesz;
		seg->memSize = phdr->p_memsz;
		seg->align = phdr->p_align;
		seg->flags = phdr->p_flags;
	}
	return 0;
}

int
RTLinker::ProcessSections(ObjContext *ctx)
{
	Elf_Scn *scn = 0;
	while ((scn = elf_nextscn(ctx->elf, scn))) {
		ObjSection *ps = ctx->obj->AddSection();
		ps->idx = elf_ndxscn(scn);
		Elf32_Shdr *shdr;
		if ((shdr = elf32_getshdr(scn))) {
			ps->type = shdr->sh_type;
			ps->baseAddr = shdr->sh_addr;
			ps->size = shdr->sh_size;
		} else {
			LIST_DELETE(list, ps, ctx->obj->sections);
			DELETE(ps);
			Error("Cannot get section header");
			return -1;
		}
	}
	return 0;
}

int
RTLinker::BuildObjTree(char *targetName)
{
	GFile *file;
	if (!(file = uLib->GetVFS()->CreateFile(targetName, GVFS::CF_READ))) {
		Error("Failed to open target file: '%s'", targetName);
		return -1;
	}
	dynTree = CreateObject(file);
	file->Release();
	return dynTree ? 0 : -1;
}

int
RTLinker::DestroyObjTree()
{
	while (dynTree) {
		DynObject *obj = dynTree;
		/* find leaf object */
		while (!LIST_ISEMPTY(obj->deps)) {
			obj = LIST_LAST(DynObject, depList, obj->deps);
		}
		DELETE(obj);
		if (obj == dynTree) {
			dynTree = 0;
		}
	}
	return 0;
}

int
RTLinker::LoadSegments(DynObject *obj)
{
	/* Firstly get size of continuous memory range required for this object */
	vaddr_t startVa = 0, endVa = 0;
	ObjSegment *seg;
	LIST_FOREACH(ObjSegment, list, seg, obj->segments) {
		if (endVa) {
			if (seg->baseAddr < startVa) {
				startVa = seg->baseAddr;
			}
			if (seg->baseAddr + seg->memSize > endVa) {
				endVa = seg->baseAddr + seg->memSize;
			}
		} else {
			startVa = seg->baseAddr;
			endVa = startVa + seg->memSize;
		}
	}
	obj->baseAddr = startVa;
	obj->size = endVa - startVa;
	/* Try to find so much space in process virtual address space */
	if (!(obj->loadAddr = uLib->GetApp()->ReserveSpace(obj->size,
			obj->flags & DynObject::F_RELOCATABLE ? 0 : obj->baseAddr))) {
		Error("Failed to find space in the address space (0x%lx @ 0x%08lx)",
			obj->size, obj->flags & DynObject::F_RELOCATABLE ? 0 : obj->baseAddr);
		return -1;
	}
	/* Unreserve the space and map all segments */
	if (uLib->GetApp()->UnReserveSpace(obj->loadAddr)) {
		Error("Failed to unreserve address space region at 0x%08lx",
			(vaddr_t)obj->loadAddr);
		return -1;
	}
	vaddr_t offset = (vaddr_t)obj->loadAddr - obj->baseAddr;
	GFile *file = uLib->GetVFS()->CreateFile(obj->path, GVFS::CF_READ);
	if (!file) {
		Error("Failed to open target file: '%s'", obj->path.GetBuffer());
		return -1;
	}
	LIST_FOREACH(ObjSegment, list, seg, obj->segments) {
		int flags = GFile::MF_FIXED;
		if (seg->flags & PF_R) {
			flags |= GFile::MF_PROT_READ;
		}
		if (seg->flags & PF_W) {
			flags |= GFile::MF_PROT_WRITE;
		}
		if (seg->flags & PF_X) {
			flags |= GFile::MF_PROT_EXEC;
		}
		if (!(seg->map = file->Map(seg->fileSize, flags, seg->fileOffset,
			(void *)(seg->baseAddr + offset)))) {
			Error("Failed to map segment (0x%lx @ 0x%08lx)", seg->fileSize,
				seg->baseAddr + offset);
			return -1;
		}
		/* Insert BSS chunk if required */
		if (seg->memSize > roundup2(seg->fileSize, PAGE_SIZE)) {
			int prot = 0;
			if (seg->flags & PF_R) {
				prot |= MM::PROT_READ;
			}
			if (seg->flags & PF_W) {
				prot |= MM::PROT_WRITE;
			}
			if (seg->flags & PF_X) {
				prot |= MM::PROT_EXEC;
			}
			vaddr_t base = seg->baseAddr + offset + roundup2(seg->fileSize, PAGE_SIZE);
			vsize_t size = seg->memSize - roundup2(seg->fileSize, PAGE_SIZE);
			if (!uLib->GetApp()->AllocateHeap( size, prot, (void *)base, 1)) {
				Error("Failed to create BSS chunk (0x%lx @ 0x%08lx)", size, base);
				return -1;
			}
		}
	}
	file->Release();
#ifdef DEBUG
	printf("Image '%s' loaded at 0x%08lx\n", obj->path.GetBuffer(),
		(vaddr_t)obj->loadAddr);
#endif /* DEBUG */
	return 0;
}

int
RTLinker::LoadSegments()
{
	/* Firstly unmap reserved space beneath linker image */
	if (uLib->GetVFS()->UnMap((void *)RT_LINKER_START_RES)) {
		Error("Failed to unmap reserved area");
		return -1;
	}
	DynObject *obj;
	FOREACH_DYNOBJECT(obj) {
		if (LoadSegments(obj)) {
			Error("Failed to load segments for '%s'", obj->path.GetBuffer());
			return -1;
		}
	}
	return 0;
}

int
RTLinker::ObjLinkContext::Initialize()
{
	offset = (vaddr_t)obj->loadAddr - obj->baseAddr;

	/* Dynamic section */
	ObjSection *scn = obj->FindSection(SHT_DYNAMIC);
	if (!scn) {
		obj->Error("Dynamic section not found");
		return -1;
	}
	dyn = (Elf32_Dyn *)(scn->baseAddr + offset);

	/* Symbols table */
	Elf32_Dyn *dt = FindDynTag(DT_SYMTAB);
	if (dt) {
		symtab = (Elf32_Sym *)(dt->d_un.d_ptr + offset);
		if ((dt = FindDynTag(DT_SYMENT))) {
			symSize = dt->d_un.d_val;
		} else {
			symSize = sizeof(Elf32_Sym);
		}
		scn = obj->FindSectionByAddr(dt->d_un.d_ptr);
		if (!scn) {
			obj->Error("Symbol section not found (0x%08lx)", dt->d_un.d_ptr);
			return -1;
		}
		numSyms = scn->size / symSize;
	}

	/* Strings table */
	if ((dt = FindDynTag(DT_STRTAB))) {
		strtab = (char *)(dt->d_un.d_ptr + offset);
		if ((dt = FindDynTag(DT_STRSZ))) {
			strSize = dt->d_un.d_val;
		}
	}

	/* Symbols hash table */
	if ((dt = FindDynTag(DT_HASH))) {
		scn = obj->FindSectionByAddr(dt->d_un.d_ptr);
		if (!scn) {
			obj->Error("Dynamic symbols hash table section not found (0x%08lx)",
				dt->d_un.d_ptr);
			return -1;
		}
		Elf32_Word *ht = (Elf32_Word *)(dt->d_un.d_ptr + offset);
		numBuckets = *ht++;
		chainSize = *ht++;
		if ((2 + numBuckets + chainSize) * sizeof(Elf32_Word) > scn->size) {
			obj->Error("Dynamic symbols hash table section size mismatch: "
				"numBuckets = %lu, chainSize = %lu, section size is %lu",
				numBuckets, chainSize, scn->size);
			return -1;
		}
		buckets = ht;
		chain = ht + numBuckets;
	}
	return 0;
}

Elf32_Dyn *
RTLinker::ObjLinkContext::FindDynTag(Elf32_Sword tag, Elf32_Dyn *prev)
{
	Elf32_Dyn *p = prev ? prev + 1 : dyn;
	while (p->d_tag != DT_NULL) {
		if (p->d_tag == tag) {
			return p;
		}
		p++;
	}
	return 0;
}

char *
RTLinker::ObjLinkContext::GetStr(u32 idx)
{
	if (strSize && idx > strSize) {
		return 0;
	}
	return strtab + idx;
}

Elf32_Sym *
RTLinker::ObjLinkContext::GetSym(u32 idx)
{
	if (idx >= numSyms) {
		return 0;
	}
	return (Elf32_Sym *)((u8 *)symtab + idx * symSize);
}

/*
 * Find specified symbol definition in any of the involved files. pObj points to
 * a location where requesting object resides. It is excluded from searched
 * objects. After the symbol found the associated object written back to pObj.
 * Firstly GLOBAL symbols are searched. If nothing found then WEAK symbols are
 * searched.
 */
Elf32_Sym *
RTLinker::FindSymbol(char *name, DynObject **pObj)
{
	u32 hash = elf_hash((const u8 *)name);
	Elf32_Sym *sym, *lastWeakSym = 0;
	DynObject *obj, *lastWeakObj = 0;
	FOREACH_DYNOBJECT(obj) {
		if (pObj && *pObj == obj) {
			continue;
		}
		ObjLinkContext *ctx = &obj->linkCtx;
		if (!ctx->buckets || !ctx->chain) {
			continue;
		}
		/* Use symbols hash table */
		u32 symIdx;
		for (symIdx = ctx->buckets[hash % ctx->numBuckets];
			symIdx != STN_UNDEF;
			symIdx = ctx->chain[symIdx]) {
			sym = ctx->GetSym(symIdx);
			/* Search only for GLOBAL and WEAK symbols */
			if (ELF32_ST_BIND(sym->st_info) != STB_GLOBAL &&
				ELF32_ST_BIND(sym->st_info) != STB_WEAK) {
				continue;
			}
			if (strcmp(name, ctx->GetStr(sym->st_name))) {
				continue;
			}
			/* Proceed to next object if symbol is undefined here */
			if (sym->st_shndx == SHN_UNDEF) {
				break;
			}
			if (ELF32_ST_BIND(sym->st_info) == STB_WEAK) {
				lastWeakSym = sym;
				lastWeakObj = obj;
				continue;
			}
			/* Symbol found */
			if (pObj) {
				*pObj = obj;
			}
			return sym;
		}
	}
	if (lastWeakSym) {
		if (pObj) {
			*pObj = lastWeakObj;
		}
		return lastWeakSym;
	}
	/* Nothing found */
	return 0;
}

int
RTLinker::ProcessRelEntry(ObjLinkContext *ctx, Elf32_Sword *location,
	Elf32_Word info, Elf32_Sword *addend)
{
	if (!addend) {
		addend = location;
	}
	u32 symIdx = ELF32_R_SYM(info);
	u32 type = ELF32_R_TYPE(info);
	Elf32_Sym *locSym;
	if (symIdx != STN_UNDEF) {
		locSym = ctx->GetSym(symIdx);
		if (!locSym) {
			Error("Can not get symbol with index %lu", symIdx);
			return -1;
		}
	} else {
		locSym = 0;
	}
	vaddr_t globValue;
	if (locSym) {
		DynObject *globObj = ctx->obj;
		Elf32_Sym *globSym = FindSymbol(ctx->GetStr(locSym->st_name), &globObj);
		if (globSym) {
			/* We use external WEAK symbol only if do not have any own definition */
			if (locSym->st_shndx && ELF32_ST_BIND(globSym->st_info) == STB_WEAK) {
				globValue = locSym->st_value + ctx->offset;
			} else {
				globValue = globSym->st_value + globObj->linkCtx.offset;
			}
		} else {
			if (type != R_386_COPY && locSym->st_shndx) {
				/* Use own symbol definition if exists */
				globValue = locSym->st_value + ctx->offset;
			} else if (ELF32_ST_BIND(locSym->st_info) == STB_WEAK) {
				/* Unresolved weak symbols have a zero value */
				globValue = 0;
			} else {
				Error("Cannot resolve symbol '%s'", ctx->GetStr(locSym->st_name));
				return -1;
			}
		}
	} else {
		globValue = 0;
	}

	/*
	 * Perform relocation according to "Object files/Relocation" section of
	 * ELF specification. Calculations are done according to figure 1-22.
	 */
	switch (type) {
	case R_386_NONE:
		break;
	case R_386_32:
		/* S + A */
		*location = globValue + *addend;
		break;
	case R_386_PC32:
		/* S + A - P */
		*location = globValue + *addend - (Elf32_Sword)location;
		break;
	case R_386_COPY:
		*location = *(Elf32_Sword *)globValue;
		break;
	case R_386_GLOB_DAT:
	case R_386_JMP_SLOT:
		/* S */
		*location = globValue;
		break;
	case R_386_RELATIVE:
		/* B + A */
		*location = ctx->offset + *addend;
		break;
	default:
		Error("Relocation type not supported: %lu", type);
		return -1;
	}
	return 0;
}

int
RTLinker::ProcessRelTable(ObjLinkContext *ctx, void *table, u32 numEntries,
		u32 entrySize, int hasAddend)
{
	Elf32_Sword *pAddend = 0;
	while (numEntries) {
		vaddr_t location;
		Elf32_Word info;
		if (hasAddend) {
			Elf32_Rela *re = (Elf32_Rela *)table;
			location = re->r_offset + ctx->offset;
			info = re->r_info;
			pAddend = &re->r_addend;
		} else {
			Elf32_Rel *re = (Elf32_Rel *)table;
			location = re->r_offset + ctx->offset;
			info = re->r_info;
		}
		if (ProcessRelEntry(ctx, (Elf32_Sword *)location, info, pAddend)) {
			return -1;
		}
		table = (u8 *)table + entrySize;
		numEntries--;
	}
	return 0;
}

/* Process all relocation entries of the object */
int
RTLinker::LinkObject(ObjLinkContext *ctx)
{
	Elf32_Dyn *dt;

	/* DT_RELA */
	if ((dt = ctx->FindDynTag(DT_RELA))) {
		void *table = (void *)(dt->d_un.d_ptr + ctx->offset);
		if (!(dt = ctx->FindDynTag(DT_RELAENT))) {
			Error("DT_RELAENT tag not found");
			return -1;
		}
		u32 entrySize = dt->d_un.d_val;
		if (!(dt = ctx->FindDynTag(DT_RELASZ))) {
			Error("DT_RELASZ tag not found");
			return -1;
		}
		if (ProcessRelTable(ctx, table, dt->d_un.d_val / entrySize, entrySize, 1)) {
			return -1;
		}
	}

	/* DT_REL */
	if ((dt = ctx->FindDynTag(DT_REL))) {
		void *table = (void *)(dt->d_un.d_ptr + ctx->offset);
		if (!(dt = ctx->FindDynTag(DT_RELENT))) {
			Error("DT_RELENT tag not found");
			return -1;
		}
		u32 entrySize = dt->d_un.d_val;
		if (!(dt = ctx->FindDynTag(DT_RELSZ))) {
			Error("DT_RELSZ tag not found");
			return -1;
		}
		if (ProcessRelTable(ctx, table, dt->d_un.d_val / entrySize, entrySize, 0)) {
			return -1;
		}
	}

	/* DT_JMPREL */
	if ((dt = ctx->FindDynTag(DT_JMPREL))) {
		void *table = (void *)(dt->d_un.d_ptr + ctx->offset);

		if (!(dt = ctx->FindDynTag(DT_PLTREL))) {
			Error("DT_PLTREL tag not found");
			return -1;
		}
		int hasAddend;
		u32 entrySize;
		if (dt->d_un.d_val == DT_REL) {
			entrySize = sizeof(Elf32_Rel);
			hasAddend = 0;
		} else if (dt->d_un.d_val == DT_RELA) {
			entrySize = sizeof(Elf32_Rela);
			hasAddend = 1;
		} else {
			Error("Invalid value in DT_PLTREL tag: %lu", dt->d_un.d_val);
			return -1;
		}

		if (!(dt = ctx->FindDynTag(DT_PLTRELSZ))) {
			Error("DT_PLTRELSZ tag not found");
			return -1;
		}
		if (ProcessRelTable(ctx, table, dt->d_un.d_val / entrySize, entrySize,
				hasAddend)) {
			return -1;
		}
	}
	return 0;
}

int
RTLinker::LinkObjects()
{
	/* Firstly initialize link contexts for all objects */
	DynObject *obj;
	FOREACH_DYNOBJECT(obj) {
		if (obj->linkCtx.Initialize()) {
			CString s;
			GetDepChain(obj, s);
			Error("Failed to initialize object linking context: '%s'",
				s.GetBuffer());
			return -1;
		}
	}

	FOREACH_DYNOBJECT(obj) {
		if (LinkObject(&obj->linkCtx)) {
			CString s;
			GetDepChain(obj, s);
			Error("Failed to link object '%s'", s.GetBuffer());
			return -1;
		}
	}
	return 0;
}

int
RTLinker::Link()
{
	if (elf_version(EV_CURRENT) == EV_NONE) {
		 Error("ELF library initialization failed: %s", elf_errmsg());
		 return -1;
	}

	CString args = GETSTR(uLib->GetProcess(), GProcess::GetArgs);
	CString target;
	int idx;
	if (!args.GetToken(target, 0, &idx)) {
		Error("No target binary provided in the arguments");
		return -1;
	}
	/* Strip target binary name from arguments */
	int len = args.GetLength();
	while (idx < len) {
		char c = args[idx];
		if (c != ' ' && c != '\t') {
			break;
		}
		idx++;
	}
	CString newArgs;
	if (idx < len) {
		args.SubStr(newArgs, idx);
	}
	uLib->GetProcess()->SetArgs(newArgs.GetBuffer());

	if (BuildObjTree(target.GetBuffer())) {
		Error("Failed to build dynamic objects tree");
		return -1;
	}

	if (LoadSegments()) {
		Error("Failed to load segments in the process memory");
		return -1;
	}

	if (LinkObjects()) {
		Error("Failed to link loaded objects");
		return -1;
	}
	return 0;
}

vaddr_t
RTLinker::GetEntry()
{
	if (!dynTree) {
		return 0;
	}
	return dynTree->entry;
}
