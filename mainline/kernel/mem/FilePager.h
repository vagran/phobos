/*
 * /phobos/kernel/mem/FilePager.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef FILEPAGER_H_
#define FILEPAGER_H_
#include <sys.h>
phbSource("$Id$");

class FilePager : public MM::Pager {
private:
	VFS::File *file;
public:
	FilePager(vsize_t size, Handle handle = 0);
	virtual ~FilePager();

	virtual int HasPage(vaddr_t offset);
	virtual int GetPage(MM::Page *pg, vaddr_t offset);
	virtual int PutPage(MM::Page *pg, vaddr_t offset);
};

#endif /* FILEPAGER_H_ */
