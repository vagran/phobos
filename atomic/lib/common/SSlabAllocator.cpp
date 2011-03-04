/*
 * /phobos/lib/common/SSlabAllocator.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#include <sys.h>
phbSource("$Id$");

#include <SSlabAllocator.h>

/* Scalable slab allocator */

SSlabAllocator::SSlabAllocator(Client *client)
{
	this->client = client;
	LIST_INIT(slabs);
	numSlabs = 0;

	/* Initialize buckets for 'small' class */
	memset(smallBuckets, 0, sizeof(smallBuckets));
	for (int i = 0; i < SMALL_NUM; i++) {
		SmallBucket *bucket = &smallBuckets[i];
		LIST_INIT(bucket->freeBlocks);
		LIST_INIT(bucket->fitableSlabs);
	}
#ifdef UNIT_TEST
	ensure(!UnitTest());
#endif
}

SSlabAllocator::~SSlabAllocator()
{
	/* free all slabs */
	SlabHeader *slab;
	client->Lock();
	while ((slab = LIST_FIRST(SlabHeader, list, slabs))) {
		LIST_DELETE(list, slab, slabs);
		client->Unlock();
		client->mfree(slab);
		client->Lock();
	}
	client->Unlock();
}

int
SSlabAllocator::GetBucketIndex(u32 size, SizeClass *pCls, u32 *pSize)
{
	SizeClass cls;
	int idx = -1;

	size = roundup2(size, ALLOC_GRANULARITY);
	if (size <= SMALL_RANGE3_HIGHEST) {
		cls = CLS_SMALL;
	} else if (size <= LARGE_HIGHEST) {
		cls = CLS_LARGE;
	} else {
		cls = CLS_HUGE;
	}

	switch (cls) {
	case CLS_SMALL:
		size = roundup2(size, SMALL_RANGE1_STEP);
		if (size <= SMALL_RANGE1_HIGHEST) {
			idx = (size - SMALL_RANGE1_FIRST) / SMALL_RANGE1_STEP;
			break;
		}
		size = roundup2(size, SMALL_RANGE2_STEP);
		if (size <= SMALL_RANGE2_HIGHEST) {
			idx = (size - SMALL_RANGE2_FIRST) / SMALL_RANGE2_STEP +
				SMALL_RANGE2_IDX;
			break;
		}
		size = roundup2(size, SMALL_RANGE3_STEP);
		idx = (size - SMALL_RANGE3_FIRST) / SMALL_RANGE3_STEP +
			SMALL_RANGE3_IDX;
		break;
	case CLS_LARGE:
		size = roundup2(size, LARGE_STEP);
		idx = size / LARGE_STEP;
		break;
	case CLS_HUGE:
		idx = 0; /* doesn't matter anything in 'Huge' class */
		break;
	}

	if (pCls) {
		*pCls = cls;
	}
	if (pSize) {
		*pSize = size;
	}
	return idx;
}

u32
SSlabAllocator::GetBucketSize(SizeClass cls, int idx)
{
	switch (cls) {
	case CLS_SMALL:
		if (idx >= SMALL_RANGE3_IDX) {
			return SMALL_RANGE3_FIRST + (idx - SMALL_RANGE3_IDX) * SMALL_RANGE3_STEP;
		}
		if (idx >= SMALL_RANGE2_IDX) {
			return SMALL_RANGE2_FIRST + (idx - SMALL_RANGE2_IDX) * SMALL_RANGE2_STEP;
		}
		return SMALL_RANGE1_FIRST + (idx - SMALL_RANGE1_IDX) * SMALL_RANGE1_STEP;
	case CLS_LARGE:
		return idx * LARGE_STEP;
	case CLS_HUGE:
		//notimpl
		return 0;
	}
	return 0;
}

#ifdef UNIT_TEST
int
SSlabAllocator::UnitTest()
{
	SizeClass cls;
	u32 size, bSize;

	for (size = ALLOC_GRANULARITY; size < SMALL_RANGE3_HIGHEST;
		size += ALLOC_GRANULARITY) {
		int idx = GetBucketIndex(size, &cls, &bSize);
		assert(idx >= 0 && idx < SMALL_NUM);
		assert(cls == CLS_SMALL);
		assert(bSize >= size);
		bSize = GetBucketSize(CLS_SMALL, idx);
		assert(bSize && bSize >= size);
	}
	for (int i = 0; i < SMALL_NUM; i++) {
		size = GetBucketSize(CLS_SMALL, i);
		assert(size);

		int idx = GetBucketIndex(size, &cls, &bSize);
		assert(idx >= 0 && idx < SMALL_NUM);
		assert(cls == CLS_SMALL);
		assert(bSize == size);
	}
	return 0;
}
#endif

SSlabAllocator::SlabHeader *
SSlabAllocator::CreateSlab()
{
	SlabHeader *slab = (SlabHeader *)client->malloc(SLAB_SIZE);
	if (!slab) {
		return 0;
	}
	assert(!((u32)slab & (PAGE_SIZE - 1)));
#ifdef DEBUG_MALLOC
	memset(slab, 0xcc, SLAB_SIZE);
#endif
	slab->numAllocated = 0;
	slab->numFree = 0;
	slab->unallocatedOffs = sizeof(SlabHeader);
	client->Lock();
	LIST_ADD(list, slab, slabs);
	numSlabs++;
	slab->fitIdx = 0xff;
	client->Unlock();
	return slab;
}

void
SSlabAllocator::FreeSlab(SlabHeader *slab)
{
	client->Lock();
	if (slab->fitIdx != 0xff) {
		LIST_DELETE(fitList, slab, smallBuckets[slab->fitIdx].fitableSlabs);
	}
	LIST_DELETE(list, slab, slabs);
	numSlabs--;
	client->Unlock();
}

void *
SSlabAllocator::malloc(u32 size)
{
	SizeClass cls;
	int idx = GetBucketIndex(size, &cls, &size);
	if (idx < 0) {
		return 0;
	}
	if (cls != CLS_SMALL) {
		/* Allocate directly from the client */
		size = roundup2(size + sizeof(BlockHeader), PAGE_SIZE);
		BlockHeader *b = (BlockHeader *)client->malloc(size);
		if (!b) {
			return 0;
		}
		b->large.cls = cls;
		return b + 1;
	}
	SmallBucket *bucket = &smallBuckets[idx];
	client->Lock();
	/* take free block if available */
	FreeBlock *fblock = LIST_FIRST(FreeBlock, list, bucket->freeBlocks);
	if (fblock) {
		assert((((BlockHeader *)fblock) - 1)->small.cls == cls &&
			(((BlockHeader *)fblock) - 1)->small.index == idx);
		assert(fblock->slab->numFree);
		fblock->slab->numFree--;
		LIST_DELETE(list, fblock, bucket->freeBlocks);
		client->Unlock();
		return fblock;
	}

	/* Try to find partially filled slab */
	SlabHeader *slab;
	int slabIdx = idx;
	while (!(slab = LIST_FIRST(SlabHeader, fitList, bucket->fitableSlabs))) {
		slabIdx++;
		if (slabIdx >= SMALL_NUM) {
			/* no suitable slab found */
			break;
		}
		bucket = &smallBuckets[slabIdx];
	}

	if (slab) {
		assert(slabIdx == slab->fitIdx);
		assert(SLAB_SIZE - slab->unallocatedOffs - sizeof(BlockHeader) >= size);
		LIST_DELETE(fitList, slab, bucket->fitableSlabs);
		slab->fitIdx = 0xff;
		bucket->numFreeSlabs--;
	} else {
		/* Allocate new slab */
		client->Unlock();
		slab = CreateSlab();
		if (!slab) {
			return 0;
		}
		client->Lock();
	}

	/* Allocate new block in the slab unallocated space */
	assert((u32)SLAB_SIZE - slab->unallocatedOffs >= (u32)sizeof(BlockHeader) + size);
	BlockHeader *b = (BlockHeader *)((u8 *)slab + slab->unallocatedOffs);
	assert(SLAB_SIZE - slab->unallocatedOffs - sizeof(BlockHeader) >= size);
#ifdef DEBUG_MALLOC
	memset(b + 1, 0xcc, size);
#endif
	b->small.cls = cls;
	b->small.index = idx;
	b->small.page = ((vaddr_t)b - (vaddr_t)slab) >> PAGE_SHIFT;
	slab->numAllocated++;
	slab->unallocatedOffs += sizeof(BlockHeader) + size;

	if (slab->unallocatedOffs &&
		(SLAB_SIZE - slab->unallocatedOffs >= (int)sizeof(BlockHeader) + ALLOC_GRANULARITY)) {
		u32 bucketSize;
		u32 availSize = SLAB_SIZE - slab->unallocatedOffs - sizeof(BlockHeader);
		int idx = GetBucketIndex(availSize, &cls, &bucketSize);
		if (idx >= 0) {
			if (cls == CLS_SMALL) {
				assert(bucketSize <= availSize || idx);
				slab->fitIdx = bucketSize > availSize ? idx - 1 : idx;
			} else {
				slab->fitIdx = SMALL_NUM - 1;
			}
			ListHead *head = &smallBuckets[slab->fitIdx].fitableSlabs;
			LIST_ADD(fitList, slab, *head);
			smallBuckets[slab->fitIdx].numFreeSlabs++;
		}
	}
	client->Unlock();
	return b + 1;
}

void
SSlabAllocator::mfree(void *p)
{
	BlockHeader *b = ((BlockHeader *)p) - 1;
	if (b->small.cls != CLS_SMALL) {
		client->mfree(b);
		return;
	}
	SlabHeader *slab = (SlabHeader *)(rounddown2((vaddr_t)b, PAGE_SIZE) -
		b->small.page * PAGE_SIZE);
	ensure(b->small.index < SMALL_NUM);
	u32 size = GetBucketSize(CLS_SMALL, b->small.index);
	ensure(size);
	SmallBucket *bucket = &smallBuckets[b->small.index];
#ifdef MALLOC_DEBUG
	memset(p, 0xfe, size);
#endif
	FreeBlock *fb = (FreeBlock *)p;
	fb->slab = slab;
	client->Lock();
	LIST_ADD(list, fb, bucket->freeBlocks);
	bucket->numFreeBlocks++;
	slab->numFree++;
	if (slab->numFree == slab->numAllocated) {
		/*
		 * XXX could either free slab or delete all free blocks from free blocks
		 * list and reset unallocated space pointer.
		 */
	}
	client->Unlock();
}

void *
SSlabAllocator::mrealloc(void *p, u32 size)
{
	BlockHeader *b = ((BlockHeader *)p) - 1;
	if (b->small.cls != CLS_SMALL) {
		return client->mrealloc(p, size);
	}
	u32 blockSize = GetBucketSize(CLS_SMALL, b->small.index);
	if (size <= blockSize) {
		return p;
	}
	void *newBlock = malloc(size);
	if (!newBlock) {
		mfree(p);
		return 0;
	}
	memcpy(newBlock, p, blockSize);
	mfree(p);
	return newBlock;
}
