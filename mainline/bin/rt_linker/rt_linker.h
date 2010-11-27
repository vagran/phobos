/*
 * /phobos/bin/rt_linker/rt_linker.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#ifndef RT_LINKER_H_
#define RT_LINKER_H_
#include <sys.h>
phbSource("$Id$");

#include <libelf.h>

class RTLinker : public Object {
private:
	/* Loadable segments */
	typedef struct {
		ListEntry list;
		off_t fileOffset;
		vaddr_t baseAddr;
		u32 fileSize, memSize;
		u32 align;
		u32 flags;
	} ObjSegment;

	/* Sections */
	typedef struct {
		vaddr_t baseAddr;
		vsize_t size;
	} ObjSection;

	struct DynObject_s;
	typedef struct DynObject_s DynObject;
	struct DynObject_s {
		enum Flags {
			F_RELOCATABLE =		0x1,
		};
		ListEntry depList;
		ListHead deps; /* dependencies */
		int numDeps;
		CString path;
		DynObject *parent;
		ListHead segments;
		struct {
			ObjSection plt, dyn, hash, dynsym, dynstr, rel_dyn, rel_plt,
				data_rel_ro, got, got_plt;
		} scns;
		u32 flags;

		DynObject_s(DynObject *parent);
		~DynObject_s();
		ObjSegment *AddSegment();
	};

	class ObjContext {
	public:
		GFile *file;
		Elf *elf;
		Elf_Scn *dynScn;
		Elf_Data *lastDTBlock;
		DynObject *obj;
		Elf_Scn *dynStrScn;
		Elf32_Ehdr *ehdr;
		Elf32_Phdr *phdr;

		ObjContext(GFile *file = 0) {
			if (file) {
				file->AddRef();
			}
			this->file = file;
			elf = 0;
			dynScn = 0;
			dynStrScn = 0;
			lastDTBlock = 0;
			obj = 0;
			ehdr = 0;
			phdr = 0;
		}

		~ObjContext() {
			if (file) {
				file->Release();
			}
			if (elf) {
				elf_end(elf);
			}
			if (obj) {
				DELETE(obj);
			}
		}

		DynObject *PopObject() {
			DynObject *obj = this->obj;
			this->obj = 0;
			return obj;
		}
	};

	DynObject *dynTree;

	DynObject *CreateObject(GFile *file, DynObject *parent = 0);
	int BuildObjTree(char *targetName);
	Elf_Scn *FindSection(Elf *elf, const char *name);
	Elf_Scn *FindSection(Elf *elf, Elf32_Word type);
	Elf_Scn *FindSectionByAddr(Elf *elf, Elf32_Word addr);
	int ProcessObjDeps(ObjContext *ctx);
	Elf32_Dyn *FindDynTag(ObjContext *ctx, Elf32_Sword tag, Elf32_Dyn *prev = 0);
	int GetDepChain(DynObject *obj, CString &s);
	DynObject *GetNextObject(DynObject *prev = 0);
	int DestroyObjTree();
	int ProcessSegments(ObjContext *ctx);
	int ProcessSections(ObjContext *ctx);
	int ProcessSection(ObjContext *ctx, const char *name, ObjSection *scn);
public:
	RTLinker();
	~RTLinker();

	int Link();
	void Error(const char *msg, ...) __format(printf, 2, 3);
};

#endif /* RT_LINKER_H_ */
