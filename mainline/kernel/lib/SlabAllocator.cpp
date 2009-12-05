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
	assert(client);
	client->Lock();
	LIST_INIT(slabGroups);
	this->initialPool = (u8 *)initialPool;
	this->initialPoolSize = initialPoolSize;
	initialPoolPtr = 0;
	this->client = client;
	client->Unlock();
}

SlabAllocator::~SlabAllocator()
{
	SlabGroup *g;
	client->Lock();
	while ((g = LIST_FIRST(SlabGroup, list, slabGroups))) {
		FreeGroup(g);
	}
	client->Unlock();
	if (initialPool) {
		client->FreeInitialPool(initialPool, initialPoolSize);
	}
}

SlabAllocator::SlabGroup *
SlabAllocator::GetGroup(u32 size)
{
	SlabGroup *g;
	LIST_FOREACH(SlabGroup, list, g, slabGroups) {
		if (g->blockSize == size) {
			return g;
		}
	}
	/* no group in list, create new */
	if (initialPool && (initialPoolSize - initialPoolPtr) > sizeof(SlabGroup)) {
		g = (SlabGroup *)&initialPool[initialPoolPtr];
		initialPoolPtr += sizeof(SlabGroup);
		memset(g, 0, sizeof(*g));
		g->flags = F_INITIALPOOL;
	} else {
		client->Unlock();
		g = (SlabGroup *)client->AllocateStruct(sizeof(SlabGroup));
		client->Lock();
		if (!g) {
			return 0;
		}
		memset(g, 0, sizeof(*g));
	}
	LIST_INIT(g->emptySlabs);
	LIST_INIT(g->filledSlabs);
	LIST_INIT(g->fullSlabs);
	g->blockSize = size;
	LIST_ADD(list, g, slabGroups);
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
			if (!slab) {
				return 0;
			}
		}
	}
	return slab;
}

void *
SlabAllocator::Allocate(u32 size)
{
	client->Lock();
	size = roundup2(size, BLOCK_GRAN);
	Slab *slab = GetSlab(size);
	if (!slab) {
		client->Unlock();
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
	client->Unlock();
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
		client->mfree(b);
		return 0;
	}
	client->Lock();
	Slab *slab = b->slab;
	if (size && slab->blockSize != size) {
		client->Unlock();
		return -1;
	}
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
					next = LIST_NEXT(Slab, list, s, g->emptySlabs);
					if (!(s->flags & F_INITIALPOOL)) {
						FreeSlab(s);
					}
				}
			}
		}
	}
	client->Unlock();
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
	LIST_DELETE(list, g, slabGroups);
	if (!(g->flags & F_INITIALPOOL)) {
		client->FreeStruct(g, sizeof(*g));
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
