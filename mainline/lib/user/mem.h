/*
 * /phobos/lib/user/mem.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef USER_MEM_H_
#define USER_MEM_H_
#include <sys.h>
phbSource("$Id$");

class UMemAllocator : public MemAllocator {
public:
	virtual void *malloc(u32 size) { return 0; /*//notimpl*/ }
	virtual void mfree(void *p) { /*//notimpl*/ }
	virtual void *mrealloc(void *p, u32 size) { return 0; /*//notimpl*/ }
	virtual void *AllocateStruct(u32 size) { return 0; /*//notimpl*/ }
	virtual void FreeStruct(void *p, u32 size = 0) { /*//notimpl*/ }
};

#endif /* USER_MEM_H_ */
