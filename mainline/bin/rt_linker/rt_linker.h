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
		void *map; /* mapping address */
	} ObjSegment;

	/* Sections */
	typedef struct {
		ListEntry list;
		u32 idx;
		vaddr_t baseAddr;
		vsize_t size;
		Elf32_Word type;
	} ObjSection;

	struct DynObject_s;
	typedef struct DynObject_s DynObject;

	/* Linking context */
	class ObjLinkContext {
	public:
		DynObject *obj;
		vaddr_t offset; /* offset between loaded and base address of the object */
		Elf32_Dyn *dyn; /* pointer to ".dynamic" section */
		Elf32_Sym *symtab; /* dynamic symbols table for this object */
		u32 symSize; /* Symbol table entry size */
		u32 numSyms; /* number of symbols in symtab */
		char *strtab; /* Strings table */
		u32 strSize; /* Size of strings table */

		/* Symbols hash table */
		Elf32_Word numBuckets, chainSize;
		Elf32_Word *buckets, *chain;

		int Initialize();
		Elf32_Dyn *FindDynTag(Elf32_Sword tag, Elf32_Dyn *prev = 0);
		char *GetStr(u32 idx);
		Elf32_Sym *GetSym(u32 idx);

		ObjLinkContext(DynObject *obj) {
			this->obj = obj;
			dyn = 0;
			symtab = 0;
			symSize = 0;
			numSyms = 0;
			strtab = 0;
			strSize = 0;
			numBuckets = 0;
			chainSize = 0;
			buckets = 0;
			chain = 0;
		}

		~ObjLinkContext() {

		}
	};

	struct DynObject_s {
		enum Flags {
			F_RELOCATABLE =		0x1, /* Load address could be different from base address */
		};

		RTLinker *linker;
		ListEntry depList;
		ListHead deps; /* dependencies */
		int numDeps;
		CString path;
		DynObject *parent;
		ListHead segments;
		ListHead sections;
		u32 flags;
		vaddr_t baseAddr; /* base address of loadable segments */
		vsize_t size; /* total range of loadable segments */
		vaddr_t entry; /* program entry point */
		void *loadAddr; /* real load address */
		ObjLinkContext linkCtx;

		DynObject_s(RTLinker *linker, DynObject *parent);
		~DynObject_s();
		ObjSegment *AddSegment();
		ObjSection *AddSection();
		ObjSection *FindSection(Elf32_Word type, ObjSection *prev = 0);
		ObjSection *FindSectionByAddr(vaddr_t addr);
		void ErrorV(const char *msg, va_list args) __format(printf, 2, 0);
		void Error(const char *msg, ...) __format(printf, 2, 3);
	};

	/* Object initialization context */
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
	int LoadSegments(); /* Load to memory everything which must be loaded */
	int LoadSegments(DynObject *obj);
	int LinkObjects();
	int LinkObject(ObjLinkContext *ctx);
	int ProcessRelTable(ObjLinkContext *ctx, void *table, u32 numEntries,
		u32 entrySize, int hasAddend);
	int ProcessRelEntry(ObjLinkContext *ctx, Elf32_Sword *location, Elf32_Word info,
		Elf32_Sword *addend = 0);
	Elf32_Sym *FindSymbol(char *name, DynObject **pObj);
public:
	RTLinker();
	~RTLinker();

	int Link();
	vaddr_t GetEntry();
	void ErrorV(const char *msg, va_list args) __format(printf, 2, 0);
	void Error(const char *msg, ...) __format(printf, 2, 3);
};

#define FOREACH_DYNOBJECT(obj) for (obj = GetNextObject(); obj; \
	obj = GetNextObject(obj))

#endif /* RT_LINKER_H_ */
