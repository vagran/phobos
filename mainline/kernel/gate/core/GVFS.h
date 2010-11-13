/*
 * /phobos/kernel/gate/gvfs.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#ifndef GVFS_H_
#define GVFS_H_
#include <sys.h>
phbSource("$Id$");

DECLARE_GCLASS(GFile);

class GFile : public GateObject {
public:
	enum SeekFlags {
		SF_SET, /* offset from beginning */
		SF_CUR, /* offset from current position */
		SF_END, /* offset from end of file */
		SF_WHENCE_MASK =	0x7,
	};
private:
	VFS::File *file;
	off_t curPos;
	off_t size;
public:
	GFile(VFS::File *file);
	virtual ~GFile();

	virtual int Truncate();
	virtual u32 GetSize(off_t *pSize);
	virtual int Rename(const char *name);
	virtual DECL_STR_PROV(GetName);
	virtual DECL_STR_PROV(GetPath);
	virtual u32 Read(void *buf, u32 len);
	virtual u32 Write(void *buf, u32 len);
	virtual u32 Seek(off_t offset, u32 flags = SF_SET, off_t *newOffset = 0); /* return new position */
	virtual u32 GetCurPos(off_t *pos = 0);
	virtual VFS::Node::Type GetType();
	virtual int Eof();

	DECLARE_GCLASS_IMP(GFile);
};

DECLARE_GCLASS(GVFS);

class GVFS : public GateObject {
public:
	enum CreateFlags {
		/* Open existing file only, CreateFile() will fail if file not exist,
		 * if the flag is not specified new file is created if required.
		 */
		CF_EXISTING =		0x1,
		/* Allow file reading */
		CF_READ =			0x2,
		/* Allow file writing */
		CF_WRITE =			0x4,
		/* Open file exclusively, other CreateFile() calls will fail if the file
		 * already opened.
		 */
		CF_EXCLUSIVE =		0x8,
	};
private:

public:
	GVFS();
	virtual ~GVFS();

	virtual GFile *CreateFile(const char *path, u32 flags = CF_READ | CF_WRITE,
		VFS::Node::Type type = VFS::Node::T_REGULAR);
	virtual int DeleteFile(const char *path);

	DECLARE_GCLASS_IMP(GVFS);
};

#endif /* GVFS_H_ */
