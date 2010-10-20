/*
 * /phobos/lib/user/mem.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#ifndef USER_MEM_H_
#define USER_MEM_H_
#include <sys.h>
phbSource("$Id$");

#include <SSlabAllocator.h>

class USlabAllocator : public MemAllocator {
private:
	GApp *app;

	class SSlabClient : public SSlabAllocator::Client {
	private:
		GApp *app;
	public:
		SSlabClient(GApp *app);
		~SSlabClient();

		virtual void *malloc(u32 size);
		virtual void mfree(void *p);
		virtual void Lock();
		virtual void Unlock();
	};

	SSlabClient client;
	SSlabAllocator alloc;
public:
	USlabAllocator(GApp *app);
	~USlabAllocator();

	virtual void *malloc(u32 size);
	virtual void mfree(void *p);
	virtual void *mrealloc(void *p, u32 size);
};

class UMemAllocator : public MemAllocator {
private:
	MemAllocator *alloc;
public:
	UMemAllocator();

	virtual void *malloc(u32 size) { return alloc->malloc(size); }
	virtual void *malloc(u32 size, void *param) { return alloc->malloc(size, param); }
	virtual void mfree(void *p) { alloc->mfree(p); }
	virtual void *mrealloc(void *p, u32 size) { return alloc->mrealloc(p, size); }
};

void operator delete(void *p);
void operator delete[](void *p);

#endif /* USER_MEM_H_ */
