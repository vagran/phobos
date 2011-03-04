/*
 * /phobos/kernel/sys/SSlabAllocator.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#ifndef SSLABALLOCATOR_H_
#define SSLABALLOCATOR_H_
#include <sys.h>
phbSource("$Id$");

/* Scalable slab allocator
 *
 * All memory allocations are classified to classes by size:
 * Small (stored in one slab):
 * 		16 - 240 bytes with 16 bytes step (15 buckets)
 * 		256 - 3840 bytes with 256 bytes step (15 buckets)
 * 		4096 - 47104 bytes with 2048 byte step (21 buckets)
 * One slab can contain blocks with different sizes.
 * Large:
 * 		49152 - 1096728 bytes with 4096 byte step (256 buckets)
 * Huge:
 *		more than 1096728 bytes, allocated directly from the kernel by
 *		GApp::AllocateHeap() method.
 */

class SSlabAllocator : public MemAllocator {
public:
	class Client : public MemAllocator {
	public:
		virtual void Lock() = 0;
		virtual void Unlock() = 0;
	};
private:
	enum {
		ALLOC_GRANULARITY_SHIFT =		4, /* shift bits */
		ALLOC_GRANULARITY =				1 << ALLOC_GRANULARITY_SHIFT,

		/* Highest size of blocks in each range of 'Small' class */
		SMALL_RANGE1_HIGHEST =			240,
		SMALL_RANGE2_HIGHEST =			3840,
		SMALL_RANGE3_HIGHEST =			47104,

		/* Blocks sizes steps in ranges of 'Small' class */
		SMALL_RANGE1_STEP =				16,
		SMALL_RANGE2_STEP =				256,
		SMALL_RANGE3_STEP =				2048,

		/* First size of each range of 'Small' class */
		SMALL_RANGE1_FIRST =			SMALL_RANGE1_STEP,
		SMALL_RANGE2_FIRST =			SMALL_RANGE1_HIGHEST + SMALL_RANGE1_STEP,
		SMALL_RANGE3_FIRST =			SMALL_RANGE2_HIGHEST + SMALL_RANGE2_STEP,

		/* Number of buckets in each range and of 'Small' class */
		SMALL_RANGE1_NUM =
			(SMALL_RANGE1_HIGHEST - SMALL_RANGE1_FIRST) / SMALL_RANGE1_STEP + 1,
		SMALL_RANGE2_NUM =
			(SMALL_RANGE2_HIGHEST - SMALL_RANGE2_FIRST) / SMALL_RANGE2_STEP + 1,
		SMALL_RANGE3_NUM =
			(SMALL_RANGE3_HIGHEST - SMALL_RANGE3_FIRST) / SMALL_RANGE3_STEP + 1,

		/* Starting indices for each range of 'Small' class */
		SMALL_RANGE1_IDX =				0,
		SMALL_RANGE2_IDX =				SMALL_RANGE1_IDX + SMALL_RANGE1_NUM,
		SMALL_RANGE3_IDX =				SMALL_RANGE2_IDX + SMALL_RANGE2_NUM,

		SMALL_NUM =						SMALL_RANGE1_NUM + SMALL_RANGE2_NUM + SMALL_RANGE3_NUM,

		LARGE_FIRST =					SMALL_RANGE3_HIGHEST + SMALL_RANGE1_STEP,
		LARGE_STEP =					4096,
		LARGE_HIGHEST =					1096728,

		SLAB_SIZE =						0x10000,
	};

	enum SizeClass {
		CLS_SMALL,
		CLS_LARGE,
		CLS_HUGE
	};

	typedef struct {
		ListEntry list, fitList;
		u8 fitIdx; /* index of the bucket where the slab is inserted, 0xff if none */
		u8 reserved;
		u16 numAllocated; /* number of blocks allocated in this slab */
		u16 numFree; /* currently free blocks in this slab */
		u16 unallocatedOffs; /* offset of the unallocated area in slab, no space available if zero */
	} SlabHeader;

	typedef union {
		struct Small {
			u8	cls :2, /* Block class */
				page :4, /* Page index in slab */
				reserved1: 2;
			u8	index :6, /* Bucket index */
				reserved2: 2;
		} small;

		struct Large {
			u8	cls :2;
		} large;

		struct Huge {
			u8	cls :2;
		} huge;
	} BlockHeader;

	typedef struct {
		ListEntry list; /* free blocks list */
		SlabHeader *slab;
	} FreeBlock;

	typedef struct {
		ListHead freeBlocks; /* Freed blocks of this size */
		ListHead fitableSlabs; /* Slabs with unallocated space at most for this size */
		u32 numFreeBlocks, numFreeSlabs;
	} SmallBucket;

	Client *client;
	SmallBucket smallBuckets[SMALL_NUM];
	int numSlabs;
	ListHead slabs;

	int GetBucketIndex(u32 size, SizeClass *pCls = 0, u32 *pSize = 0);
	u32 GetBucketSize(SizeClass cls, int idx);
#ifdef UNIT_TEST
	int UnitTest();
#endif
	SlabHeader *CreateSlab();
	void FreeSlab(SlabHeader *slab);
public:
	SSlabAllocator(Client *client);
	~SSlabAllocator();

	virtual void *malloc(u32 size);
	virtual void mfree(void *p);
	virtual void *mrealloc(void *p, u32 size);
};

#endif /* SSLABALLOCATOR_H_ */
