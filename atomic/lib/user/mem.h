/*
 * /phobos/lib/user/mem.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
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

#define ALLOC(type, count)		((type *)malloc(sizeof(type) * (count)))
#define FREE(p)					mfree(p)

#ifndef DEBUG_MALLOC
#define NEW(className,...)			new className(__VA_ARGS__)
#else /* DEBUG_MALLOC */
#define NEW(className,...)			new(__STR(className), __FILE__, __LINE__) className(__VA_ARGS__)
#endif /* DEBUG_MALLOC */
#define DELETE(ptr)					delete (ptr)

void *operator new(size_t size);
void *operator new(size_t size, const char *className, const char *fileName, int line);
void operator delete(void *p);
void operator delete[](void *p);

#endif /* USER_MEM_H_ */
