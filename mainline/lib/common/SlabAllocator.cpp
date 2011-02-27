/*
 * /kernel/lib/SlabAllocator.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#include <sys.h>
phbSource("$Id$");

#include <SlabAllocator.h>

SlabAllocator::SlabAllocator(SlabClient *client, void *initialPool, u32 initialPoolSize)
{
	assert(client);
	client->Lock();
	memset(slabGroups, 0, sizeof(slabGroups));
	this->initialPool = (u8 *)initialPool;
	this->initialPoolSize = initialPoolSize;
	initialPoolPtr = 0;
	this->client = client;
	client->Unlock();
}

SlabAllocator::~SlabAllocator()
{
	client->Lock();
	for (size_t i = 0; i < sizeof(slabGroups) / sizeof(slabGroups[0]); i++) {
		if (slabGroups[i]) {
			FreeGroup(slabGroups[i]);
		}
	}
	client->Unlock();
	if (initialPool) {
		client->FreeInitialPool(initialPool, initialPoolSize);
	}
}

SlabAllocator::SlabGroup *
SlabAllocator::GetGroup(u32 size)
{
	SlabGroup *g = slabGroups[size / BLOCK_GRAN];
	if (g) {
		return g;
	}
	/* no group, create new */
	if (size == sizeof(SlabGroup)) {
		g = &groupGroup;
		memset(g, 0, sizeof(*g));
		g->flags = F_INITIALPOOL;
	} else if (initialPool && (initialPoolSize - initialPoolPtr) > sizeof(SlabGroup)) {
		g = (SlabGroup *)&initialPool[initialPoolPtr];
		initialPoolPtr += sizeof(SlabGroup);
		memset(g, 0, sizeof(*g));
		g->flags = F_INITIALPOOL;
	} else {
		g = (SlabGroup *)Allocate(sizeof(SlabGroup));
		if (!g) {
			return 0;
		}
		memset(g, 0, sizeof(*g));
	}
	LIST_INIT(g->emptySlabs);
	LIST_INIT(g->filledSlabs);
	LIST_INIT(g->fullSlabs);
	g->blockSize = size;
	slabGroups[size / BLOCK_GRAN] = g;
	return g;
}

SlabAllocator::Slab *
SlabAllocator::AllocateSlab(SlabGroup *g)
{
	u32 size = sizeof(Slab) + (g->blockSize + OFFSETOF(Block, data)) * MIN_BLOCKS_IN_SLAB;
	if (size < MIN_SLAB_SIZE) {
		size = MIN_SLAB_SIZE;
	}
	size = roundup2(size, ALLOC_GRAN);
	Slab *slab;
	if (initialPool && (initialPoolSize - initialPoolPtr) >= size) {
		if (initialPoolSize - initialPoolPtr - size < MIN_SLAB_SIZE) {
			size = initialPoolSize - initialPoolPtr;
		}
		slab = (Slab *)&initialPool[initialPoolPtr];
		initialPoolPtr += size;
		memset(slab, 0, sizeof(*slab));
		slab->flags = F_INITIALPOOL;
	} else {
		client->Unlock();
		slab = (Slab *)client->malloc(size);
		client->Lock();
		if (!slab) {
			return 0;
		}
		memset(slab, 0, sizeof(*slab));
	}
	slab->blockSize = g->blockSize;
	slab->group = g;
	slab->usedBlocks = 0;
	slab->totalBlocks = (size - sizeof(Slab)) / (slab->blockSize + OFFSETOF(Block, data));
	LIST_ADD(list, slab, g->emptySlabs);
	g->numEmptySlabs++;
	g->totalBlocks += slab->totalBlocks;
	for (u32 i = 0; i < slab->totalBlocks; i++) {
		Block *b = (Block *)(((u8 *)slab) + sizeof(Slab) +
			i * (g->blockSize + OFFSETOF(Block, data)));
		b->slab = slab;
		LIST_ADD(list, b, slab->freeBlocks);
	}
	return slab;
}

SlabAllocator::Slab *
SlabAllocator::GetSlab(u32 size)
{
	SlabGroup *g = GetGroup(size);
	if (!g) {
		return 0;
	}

	Slab *slab = LIST_FIRST(Slab, list, g->filledSlabs);
	if (!slab) {
		slab = LIST_FIRST(Slab, list, g->emptySlabs);
		if (!slab) {
			/* need new slab */
			slab = AllocateSlab(g);
		}
	}
	return slab;
}

void *
SlabAllocator::Allocate(u32 size)
{
	size = roundup2(size, BLOCK_GRAN);
	Slab *slab = GetSlab(size);
	if (!slab) {
		return 0;
	}
	SlabGroup *g = slab->group;

	Block *b = LIST_FIRST(Block, list, slab->freeBlocks);
	LIST_DELETE(list, b, slab->freeBlocks);
	if (slab->usedBlocks) {
		slab->usedBlocks++;
		assert(slab->usedBlocks <= slab->totalBlocks);
		if (slab->usedBlocks == slab->totalBlocks) {
			LIST_DELETE(list, slab, g->filledSlabs);
			LIST_ADD(list, slab, g->fullSlabs);
		}
	} else {
		slab->usedBlocks++;
		LIST_DELETE(list, slab, g->emptySlabs);
		g->numEmptySlabs--;
		if (slab->usedBlocks < slab->totalBlocks) {
			LIST_ADD(list, slab, g->filledSlabs);
		} else {
			LIST_ADD(list, slab, g->fullSlabs);
		}
	}
	g->usedBlocks++;
	return &b->data[0];
}

int
SlabAllocator::Free(void *p, u32 size)
{
	size = roundup2(size, BLOCK_GRAN);
	Block *b = (Block *)(((u8 *)p) - OFFSETOF(Block, data));
	if (b->type == BT_INITIAL) {
		return 0;
	}
	if (b->type == BT_DIRECT) {
		client->Unlock();
		client->mfree(b);
		client->Lock();
		return 0;
	}
	Slab *slab = b->slab;
	if (size && slab->blockSize < size) {
		return -1;
	}
#ifdef DEBUG
	memset(p, 0xfe, slab->blockSize);
#endif /* DEBUG */
	SlabGroup *g = slab->group;
	LIST_ADD(list, b, slab->freeBlocks);
	assert(slab->usedBlocks);
	assert(slab->usedBlocks <= slab->totalBlocks);
	if (slab->usedBlocks == slab->totalBlocks) {
		LIST_DELETE(list, slab, g->fullSlabs);
		slab->usedBlocks--;
		g->usedBlocks--;
		if (slab->usedBlocks) {
			LIST_ADD(list, slab, g->filledSlabs);
		} else {
			LIST_ADD(list, slab, g->emptySlabs);
			g->numEmptySlabs++;
		}
	} else {
		slab->usedBlocks--;
		g->usedBlocks--;
		if (!slab->usedBlocks) {
			LIST_DELETE(list, slab, g->filledSlabs);
			LIST_ADD(list, slab, g->emptySlabs);
			g->numEmptySlabs++;

			if (g->numEmptySlabs > MAX_EMPTY_SLABS) {
				Slab *s, *next = LIST_FIRST(Slab, list, g->emptySlabs);
				for (u32 i = 0; i < EMPTY_SLABS_HYST && next; i++) {
					s = next;
					next = LIST_NEXT(Slab, list, s);
					if (!(s->flags & F_INITIALPOOL)) {
						FreeSlab(s);
					}
				}
			}
		}
	}
	return 0;
}

/* called with lock */
void
SlabAllocator::FreeSlab(Slab *slab)
{
	assert(!slab->usedBlocks);
	if (slab->flags & F_INITIALPOOL) {
		return;
	}
	LIST_DELETE(list, slab, slab->group->emptySlabs);
	slab->group->numEmptySlabs--;
	slab->group->totalBlocks -= slab->totalBlocks;
	client->Unlock();
	client->mfree(slab);
	client->Lock();
}

void
SlabAllocator::FreeGroup(SlabGroup *g)
{
	Slab *slab;
	/* move all slabs to empty slabs list */
	while ((slab = LIST_FIRST(Slab, list, g->fullSlabs))) {
		g->usedBlocks -= slab->usedBlocks;
		slab->usedBlocks = 0;
		LIST_DELETE(list, slab, g->fullSlabs);
		LIST_ADD(list, slab, g->emptySlabs);
	}
	while ((slab = LIST_FIRST(Slab, list, g->filledSlabs))) {
		g->usedBlocks -= slab->usedBlocks;
		slab->usedBlocks = 0;
		LIST_DELETE(list, slab, g->filledSlabs);
		LIST_ADD(list, slab, g->emptySlabs);
	}
	/* now can free each slab */
	while ((slab = LIST_FIRST(Slab, list, g->emptySlabs))) {
		if (slab->flags & F_INITIALPOOL) {
			LIST_DELETE(list, slab, g->emptySlabs);
		} else {
			FreeSlab(slab);
		}
	}
	slabGroups[g->blockSize / BLOCK_GRAN] = 0;
	if (!(g->flags & F_INITIALPOOL)) {
		Free(g, sizeof(*g));
	}
}

void *
SlabAllocator::malloc(u32 size)
{
	Block *b;

	size = roundup2(size, BLOCK_GRAN) + OFFSETOF(Block, data);
	client->Lock();
	if (size <= initialPoolSize - initialPoolPtr) {
		b = (Block *)&initialPool[initialPoolPtr];
		initialPoolPtr += size;
		client->Unlock();
		b->type = BT_INITIAL;
		return b->data;
	}
	client->Unlock();
	b = (Block *)client->malloc(size);
	if (!b) {
		return 0;
	}
	b->type = BT_DIRECT;
	return b->data;
}

void
SlabAllocator::mfree(void *p)
{
	client->Lock();
	Free(p);
	client->Unlock();
}

void *
SlabAllocator::AllocateStruct(u32 size)
{
	if (size > MAX_BLOCK_SIZE) {
		return malloc(size);
	}
	client->Lock();
	void *p = Allocate(size);
	client->Unlock();
	return p;
}

void
SlabAllocator::FreeStruct(void *p, u32 size)
{
	if (size > MAX_BLOCK_SIZE) {
		return mfree(p);
	}
	client->Lock();
	Free(p, size);
	client->Unlock();
}
