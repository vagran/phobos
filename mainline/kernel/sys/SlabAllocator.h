/*
 * /kernel/sys/SlabAllocator.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef SLABALLOCATOR_H_
#define SLABALLOCATOR_H_
#include <sys.h>
phbSource("$Id$");

#include <MemAllocator.h>

class SlabAllocator : public MemAllocator {
public:
	class SlabClient : public MemAllocator {
	public:
		virtual void FreeInitialPool(void *p, u32 size) = 0;
		virtual void Lock() {}
		virtual void Unlock() {}
	};
private:
	typedef enum {
		MIN_SLAB_SIZE = PAGE_SIZE,
		MIN_BLOCKS_IN_SLAB = 64,
		ALLOC_GRAN = PAGE_SIZE, /* must be power of 2 */
		BLOCK_GRAN = 4, /* must be power of 2 */
		MAX_EMPTY_SLABS = 8,
		EMPTY_SLABS_HYST = 4,
		MAX_BLOCK_SIZE = 0x1000,
	} AllocPolicy;

	typedef enum {
		F_INITIALPOOL =		0x1,
	} Flags;

	typedef struct {
		ListEntry list;
		u32 blockSize;
		u32 flags;
		u32 totalBlocks, usedBlocks;
		u32 numEmptySlabs;
		ListHead emptySlabs, filledSlabs, fullSlabs;
	} SlabGroup;

	typedef struct {
		ListEntry list;
		u32 blockSize;
		u32 totalBlocks, usedBlocks;
		u32 flags;
		ListHead freeBlocks;
		SlabGroup *group;
	} Slab;

	typedef enum {
		BT_INITIAL, /* allocated in initial pool */
		BT_DIRECT, /* allocated by malloc from client */
	} BlockType;

	typedef struct {
		union {
			Slab *slab;
			BlockType type;
		};
		union {
			struct {
				ListEntry list;
			};
			u8 data[1];
		};
	} Block;

	SlabGroup *slabGroups[MAX_BLOCK_SIZE / BLOCK_GRAN];
	SlabGroup groupGroup;
	u8 *initialPool;
	u32 initialPoolSize;
	u32 initialPoolPtr;
	SlabClient *client;

	void *Allocate(u32 size); /* must be called locked */
	int Free(void *p, u32 size = 0); /* must be called locked */
	Slab *GetSlab(u32 size);
	SlabGroup *GetGroup(u32 size);
	Slab *AllocateSlab(SlabGroup *g);
	void FreeSlab(Slab *slab);
	void FreeGroup(SlabGroup *g);
public:
	SlabAllocator(SlabClient *client, void *initialPool = 0, u32 initialPoolSize = 0) __noinline;
	virtual ~SlabAllocator();

	virtual void *malloc(u32 size);
	virtual void mfree(void *p);
	virtual void *AllocateStruct(u32 size);
	virtual void FreeStruct(void *p, u32 size = 0);
};

#endif /* SLABALLOCATOR_H_ */
