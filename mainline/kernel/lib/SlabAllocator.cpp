/*
 * /kernel/lib/SlabAllocator.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <SlabAllocator.h>

SlabAllocator::SlabAllocator(SlabClient *client, void *initialPool, u32 initialPoolSize)
{
	LIST_INIT(slabGroups);
	this->initialPool = (u8 *)initialPool;
	this->initialPoolSize = initialPoolSize;
	this->client = client;
}

SlabAllocator::~SlabAllocator()
{
	if (initialPool && client) {
		client->FreeInitialPool(initialPool, initialPoolSize);
	}
}

void *
SlabAllocator::Allocate(u32 size)
{

	return 0;
}

int
SlabAllocator::Free(void *p, u32 size)
{

	return 0;
}

void *
SlabAllocator::malloc(u32 size)
{
	return Allocate(size);
}

void
SlabAllocator::mfree(void *p)
{
	Free(p);
}

void *
SlabAllocator::AllocateStruct(u32 size)
{
	return Allocate(size);
}

void
SlabAllocator::FreeStruct(void *p, u32 size)
{
	Free(p, size);
}
