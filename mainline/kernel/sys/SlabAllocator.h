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
	};
private:
	typedef enum {
		MIN_SLAB_SIZE = PAGE_SIZE,
		MIN_BLOCKS_IN_SLAB = 32,

	} AllocPolicy;

	typedef struct {
		ListEntry list;
		u32 blockSize;
		u32 totalBlocks, allocatedBlocks;
		ListHead freeBlocks;
	} Slab;

	typedef struct {
		Slab *slab;
		union {
			struct {
				ListEntry list;
			};
			u8 data[1];
		};
	} Block;

	typedef struct {
		ListEntry list;
		u32 blockSize;
		ListHead emptySlabs, filledSlabs, fullSlabs;
	} SlabGroup;

	ListHead slabGroups;
	u8 *initialPool;
	u32 initialPoolSize;
	SlabClient *client;

	void *Allocate(u32 size);
	int Free(void *p, u32 size = 0);
public:
	SlabAllocator(SlabClient *client, void *initialPool = 0, u32 initialPoolSize = 0);
	virtual ~SlabAllocator();

	virtual void *malloc(u32 size);
	virtual void mfree(void *p);
	virtual void *AllocateStruct(u32 size);
	virtual void FreeStruct(void *p, u32 size);
};

#endif /* SLABALLOCATOR_H_ */
