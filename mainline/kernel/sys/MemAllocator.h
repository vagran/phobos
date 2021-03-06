/*
 * /kernel/sys/MemAllocator.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#ifndef MEMALLOCATOR_H_
#define MEMALLOCATOR_H_
#include <sys.h>
phbSource("$Id$");

class MemAllocator : public Object {
public:
	MemAllocator() {}
	virtual void *malloc(u32 size) = 0;
	virtual void *malloc(u32 size, void *param) { return malloc(size); }
	virtual void mfree(void *p) = 0;
	virtual void *mrealloc(void *p, u32 size) {
		void *pNew = malloc(size);
		if (pNew) {
			memcpy(pNew, p, size);
		}
		mfree(p);
		return pNew;
	}
	virtual void *AllocateStruct(u32 size) { return malloc(size); }
	virtual void FreeStruct(void *p, u32 size = 0) { return mfree(p); }
};

#endif /* MEMALLOCATOR_H_ */
