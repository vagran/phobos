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
	dynTree = 0;
}

RTLinker::~RTLinker()
{
	DestroyObjTree();
}

void
RTLinker::Error(const char *msg, ...)
{
	printf("Run-time linker: ");
	va_list va;
	va_start(va, msg);
	vprintf(msg, va);
	printf("\n");
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

RTLinker::DynObject_s::DynObject_s(DynObject *parent)
{
	LIST_INIT(deps);
	numDeps = 0;
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

	printf("Object created: %s\n", curObjName.GetBuffer());//temp

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

	if (!(ctx.obj = NEW(DynObject, parent))) {
		Error("Cannot allocate dynamic object instance");
		return 0;
	}
	ctx.obj->path = path;

	if (ProcessObjDeps(&ctx)) {
		DELETE(ctx.obj);
		ctx.obj = 0;
		Error("Cannot process object dependencies");
		return 0;
	}

	return ctx.obj;
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
RTLinker::Link()
{
	if (elf_version(EV_CURRENT) == EV_NONE) {
		 Error("ELF library initialization failed: %s", elf_errmsg());
		 return -1;
	}

	CString target = GETSTR(uLib->GetProcess(), GProcess::GetArgs);
	int len = target.Find(' ');
	if (len != -1) {
		target.Truncate(len);
	}
	if (BuildObjTree(target.GetBuffer())) {
		Error("Failed to build dynamic objects tree");
		return -1;
	}



	return 0;
}
