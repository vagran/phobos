/*
 * /kernel/lib/BuddyAllocator.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <BuddyAllocator.h>

template <typename range_t>
BuddyAllocator<range_t>::BuddyAllocator(BuddyClient *client)
{
	assert(client);
	client->Lock();
	isInitialized = 0;
	this->client = client;
	TREE_INIT(tree);
	numBlocks = 0;
	freeBlocks = 0;
	LIST_INIT(blocksPool);
	numPool = 0;
	numReqPool = 0;
	client->Unlock();
}

template <typename range_t>
BuddyAllocator<range_t>::~BuddyAllocator()
{
	client->Lock();
	FreeTree();
	isInitialized = 0;
	client->Unlock();
}

template <typename range_t>
typename BuddyAllocator<range_t>::BlockDesc *
BuddyAllocator<range_t>::AllocateBlock()
{
	if (!isInitialized) {
		return (BlockDesc *)client->AllocateStruct(sizeof(BlockDesc));
	}
	assert(!LIST_ISEMPTY(blocksPool));
	if (LIST_ISEMPTY(blocksPool)) {
		return 0;
	}
	BlockDesc *b = LIST_FIRST(BlockDesc, busyChain.list, blocksPool);
	LIST_DELETE(busyChain.list, b, blocksPool);
	numPool--;
	return b;
}

template <typename range_t>
void
BuddyAllocator<range_t>::FreeBlock(BlockDesc *b)
{
	LIST_ADD(busyChain.list, b, blocksPool);
	numPool++;
}

/* called without lock */
template <typename range_t>
int
BuddyAllocator<range_t>::KeepBlocks()
{
	client->Lock();
	if (numPool + numReqPool < BPOOL_LOWWAT || numPool + numReqPool > BPOOL_HIWAT) {
		numReqPool = (BPOOL_LOWWAT + BPOOL_HIWAT) / 2 - numPool;
	} else {
		client->Unlock();
		return 0;
	}
	while (numReqPool) {
		if (numReqPool > 0) {
			client->Unlock();
			BlockDesc *b = (BlockDesc *)client->AllocateStruct(sizeof(BlockDesc));
			client->Lock();
			if (!b) {
				break;
			}
			LIST_ADD(busyChain.list, b, blocksPool);
			numPool++;
			numReqPool--;
		} else {
			BlockDesc *b = LIST_FIRST(BlockDesc, busyChain.list, blocksPool);
			LIST_DELETE(busyChain.list, b, blocksPool);
			numPool--;
			numReqPool++;
			client->Unlock();
			client->FreeStruct(b, sizeof(BlockDesc));
			client->Lock();
		}
	}
	client->Unlock();
	return 0;
}

template <typename range_t>
int
BuddyAllocator<range_t>::Initialize(range_t base, range_t size, u16 minOrder, u16 maxOrder)
{
	client->Lock();
	assert(!isInitialized);
	if (isInitialized || maxOrder <= minOrder) {
		client->Unlock();
		return -1;
	}
	FreeTree();
	range_t end = rounddown2(base + size, (range_t)1 << minOrder);
	base = roundup2(base, (range_t)1 << minOrder);
	size = end - base;
	this->base = base;
	this->size = size;
	this->minOrder = minOrder;
	this->maxOrder = maxOrder;
	freeBlocks = (ListHead *)client->malloc((maxOrder - minOrder + 1) * sizeof(ListHead));
	for (u16 i = minOrder; i <= maxOrder; i++) {
		LIST_INIT(freeBlocks[i - minOrder]);
	}

	while (1) {
		if (end - base < (range_t)1 << minOrder) {
			break;
		}

		/* find maximal order for current base */
		int order = minOrder + 1;
		range_t bsize;
		while (1) {
			bsize = (range_t)1 << order;
			if ((base & (bsize - 1)) || bsize > size) {
				order--;
				bsize >>= 1;
				break;
			}
			if (order >= maxOrder) {
				break;
			}
			order++;
		}

		BlockDesc *b = AddBlock(order, base);
		AddFreeBlock(b);
		base += bsize;
		size -= bsize;
	}
	isInitialized = 1;
	client->Unlock();
	KeepBlocks();
	return 0;
}

template <typename range_t>
void
BuddyAllocator<range_t>::AddFreeBlock(BlockDesc *b)
{
	assert(!(b->flags & BF_FREE));
	LIST_ADD(busyChain.list, b, freeBlocks[b->order - minOrder]);
	b->flags |= BF_FREE;
}

template <typename range_t>
void
BuddyAllocator<range_t>::DeleteFreeBlock(BlockDesc *b)
{
	assert(b->flags & BF_FREE);
	LIST_DELETE(busyChain.list, b, freeBlocks[b->order - minOrder]);
	b->flags &= ~BF_FREE;
}

template <typename range_t>
int
BuddyAllocator<range_t>::Allocate(range_t size, range_t *location, void *arg)
{
	assert(isInitialized);
	if (!size || !isInitialized) {
		return -1;
	}
	KeepBlocks();
	client->Lock();
	range_t sz = size - 1;
	u16 reqOrder = 0;
	while (sz) {
		sz >>= 1;
		reqOrder++;
	}
	if (reqOrder > maxOrder) {
		return -1;
	}
	if (reqOrder < minOrder) {
		reqOrder = minOrder;
	}

	BlockDesc *b = 0;
	for (u16 order = reqOrder; order <= maxOrder; order++) {
		ListHead &head = freeBlocks[order - minOrder];
		if (!LIST_ISEMPTY(head)) {
			b = LIST_FIRST(BlockDesc, busyChain.list, head);
			assert(b->order == order);
			DeleteFreeBlock(b);
			break;
		}
	}
	if (!b) {
		/* no free blocks */
		client->Unlock();
		return -1;
	}
	if (b->order > reqOrder) {
		SplitBlock(b, reqOrder);
	}
	*location = b->node.key;

	/* make chain if required */
	range_t realSize = roundup2(size, 1 << minOrder);
	range_t blockSize = 1 << reqOrder;
	ListHead *head = &b->busyHead.chain;
	LIST_INIT(*head);
	b->busyHead.blockSize = realSize;
	b->flags |= BF_BUSY;
	b->busyHead.allocArg = arg;
	BlockDesc *headBlock = b;
	while (realSize < blockSize) {
		BlockDesc *b2 = AddBlock(b->order - 1, b->node.key + ((range_t)1 << (b->order - 1)));
		b->order--;
		if (realSize <= (blockSize >> 1)) {
			AddFreeBlock(b2);
		} else {
			realSize -= blockSize >> 1;
			b = b2;
			b->flags |= BF_BUSYCHAIN;
			b->busyChain.head = headBlock;
			LIST_ADD(busyChain.list, b, *head);
		}
		blockSize >>= 1;
	}
	client->Unlock();
	client->Allocate(*location, roundup2(size, 1 << minOrder), arg);
	return 0;
}

/* must be called locked */
template <typename range_t>
int
BuddyAllocator<range_t>::AllocateArea(range_t location, range_t size, void *arg, int reserve)
{
	range_t end = roundup2(location + size, (range_t)1 << minOrder);
	location = rounddown2(location, (range_t)1 << minOrder);
	size = end - location;
	if (!size || location < base || location + size > base + this->size) {
		return -1;
	}
	/* pass 1, verify that area is free */
	range_t loc = location;
	/* find the first block */
	u16 order = minOrder;
	BlockDesc *b = 0;
	while (order <= maxOrder) {
		b = TREE_FIND(loc, BlockDesc, node, tree);
		if (b) {
			if (!(b->flags & BF_FREE)) {
				return -1;
			}
			loc += 1 << b->order;
			break;
		}
		while (order <= maxOrder) {
			order++;
			range_t newLoc = rounddown2(loc, (range_t)1 << order);
			if (newLoc != loc) {
				loc = newLoc;
				break;
			}
		}
	}
	assert(b);
	/* scan the range */
	while (loc < end) {
		b = TREE_FIND(loc, BlockDesc, node, tree);
		assert(b);
		if (!(b->flags & BF_FREE)) {
			return -1;
		}
		loc += (range_t)1 << b->order;
	}

	/* pass 2, space is free, reserve the range */
	loc = location;
	/* find the first block */
	order = minOrder;
	b = 0;
	while (order <= maxOrder) {
		b = TREE_FIND(loc, BlockDesc, node, tree);
		if (b) {
			while (loc != location) {
				BlockDesc *b2 = AddBlock(b->order - 1, b->node.key + ((range_t)1 << (b->order - 1)));
				AddFreeBlock(b2);
				DeleteFreeBlock(b);
				b->order--;
				AddFreeBlock(b);
				if (location >= b2->node.key) {
					b = b2;
				}
				loc = b->node.key;
			}
			break;
		}
		while (order <= maxOrder) {
			order++;
			range_t newLoc = rounddown2(loc, (range_t)1 << order);
			if (newLoc != loc) {
				loc = newLoc;
				break;
			}
		}
	}
	assert(b);

	BlockDesc *head = 0;
	while (location < end) {
		range_t bsize = (range_t)1 << b->order;
		while (end - location < bsize) {
			BlockDesc *b2 = AddBlock(b->order - 1, b->node.key + ((range_t)1 << (b->order - 1)));
			AddFreeBlock(b2);
			DeleteFreeBlock(b);
			b->order--;
			AddFreeBlock(b);
			bsize = (range_t)1 << b->order;
		}
		DeleteFreeBlock(b);
		if (!head) {
			b->flags |= reserve ? BF_RESERVED : BF_BUSY;
			b->busyHead.allocArg = arg;
			b->busyHead.blockSize = size;
			head = b;
		} else {
			b->flags |= reserve ? BF_RESCHAIN : BF_BUSYCHAIN;
			b->busyChain.head = head;
			LIST_ADD(busyChain.list, b, head->busyHead.chain);
		}
		location += (range_t)1 << b->order;
		if (location < end) {
			b = TREE_FIND(location, BlockDesc, node, tree);
			assert(b);
		}
	}
	return 0;
}

template <typename range_t>
int
BuddyAllocator<range_t>::Reserve(range_t location, range_t size, void *arg)
{
	assert(isInitialized);
	if (!size || !isInitialized) {
		return -1;
	}
	KeepBlocks();
	client->Lock();
	int rc = AllocateArea(location, size, arg, 1);
	client->Unlock();
	return rc;
}

template <typename range_t>
int
BuddyAllocator<range_t>::AllocateFixed(range_t location, range_t size, void *arg)
{
	assert(isInitialized);
	if (!size || !isInitialized) {
		return -1;
	}
	KeepBlocks();
	client->Lock();
	int rc = AllocateArea(location, size, arg, 0);
	client->Unlock();
	return rc;
}

template <typename range_t>
int
BuddyAllocator<range_t>::Lookup(range_t location, range_t *pBase, range_t *pSize,
	void **pAllocArg, int flags, int *pFlags)
{
	if (location < base || location >= base + size) {
		return -1;
	}
	range_t lookupLoc;
	for (int order = minOrder; order <= maxOrder; order++) {
		range_t newLookupLoc = rounddown2(location, 1 << order);
		if (order != minOrder && newLookupLoc == lookupLoc) {
			continue;
		}
		BlockDesc *b = TREE_FIND(newLookupLoc, BlockDesc, node, tree);
		if (b) {
			assert(b->order >= order);
			if (b->flags & BF_FREE) {
				if (!(flags & LUF_FREE)) {
					return -1;
				}
				if (pFlags) {
					*pFlags = LUF_FREE;
				}
				if (pSize) {
					*pSize = 1 << b->order;
				}
			} else if (b->flags & (BF_BUSY | BF_BUSYCHAIN)) {
				if (!(flags & LUF_ALLOCATED)) {
					return -1;
				}
				if (b->flags & BF_BUSYCHAIN) {
					assert(!(b->flags & BF_BUSY));
					b = b->busyChain.head;
				}
				assert(b->flags & BF_BUSY);
				if (pFlags) {
					*pFlags = LUF_ALLOCATED;
				}
				if (pSize) {
					*pSize = b->busyHead.blockSize;
				}
				if (pAllocArg) {
					*pAllocArg = b->busyHead.allocArg;
				}
			} else if (b->flags & (BF_RESERVED | BF_RESCHAIN)) {
				if (!(flags & LUF_RESERVED)) {
					return -1;
				}
				if (b->flags & BF_RESCHAIN) {
					assert(!(b->flags & BF_RESERVED));
					b = b->busyChain.head;
				}
				assert(b->flags & BF_RESERVED);
				if (pFlags) {
					*pFlags = LUF_RESERVED;
				}
				if (pSize) {
					*pSize = b->busyHead.blockSize;
				}
				if (pAllocArg) {
					*pAllocArg = b->busyHead.allocArg;
				}
			}
			if (pBase) {
				*pBase = b->node.key;
			}
			return 0;
		}
		lookupLoc = newLookupLoc;
	}
	return -1;
}

template <typename range_t>
void
BuddyAllocator<range_t>::SplitBlock(BlockDesc *b, u16 reqOrder)
{
	while (b->order > reqOrder) {
		BlockDesc *b2 = AddBlock(b->order - 1, b->node.key + ((range_t)1 << (b->order - 1)));
		AddFreeBlock(b2);
		b->order--;
	}
}

template <typename range_t>
typename BuddyAllocator<range_t>::BlockDesc *
BuddyAllocator<range_t>::MergeBlock(BlockDesc *b)
{
	int deleted = 0;
	while (b->order < maxOrder) {
		range_t posBit = (range_t)1 << b->order;
		range_t adjLoc = b->node.key ^ posBit;
		BlockDesc *adjBlock = TREE_FIND(adjLoc, BlockDesc, node, tree);
		if (!adjBlock || !(adjBlock->flags & BF_FREE) || (adjBlock->order != b->order)) {
			break;
		}

		if (b->node.key & posBit) {
			DeleteBlock(b);
			b = adjBlock;
			deleted = 0;
		} else {
			DeleteBlock(adjBlock);
		}
		if (!deleted) {
			DeleteFreeBlock(b);
			deleted = 1;
		}
		b->order++;
	}
	if (deleted) {
		AddFreeBlock(b);
	}
	return b;
}

template <typename range_t>
typename BuddyAllocator<range_t>::BlockDesc *
BuddyAllocator<range_t>::AddBlock(u16 order, range_t location)
{
	BlockDesc *b = AllocateBlock();
	if (!b) {
		return 0;
	}
	b->order = order;
	b->flags = 0;
	b->node.key = location;
	TREE_ADD(node, b, tree);
	numBlocks++;
	return b;
}

template <typename range_t>
void
BuddyAllocator<range_t>::DeleteBlock(BlockDesc *b)
{
	if (b->flags & BF_FREE) {
		DeleteFreeBlock(b);
	}
	TREE_DELETE(node, b, tree);
	assert(numBlocks);
	numBlocks--;
	FreeBlock(b);
}

template <typename range_t>
int
BuddyAllocator<range_t>::Free(range_t location)
{
	assert(isInitialized);
	if (!isInitialized) {
		return -1;
	}
	KeepBlocks();
	client->Lock();
	BlockDesc *b = TREE_FIND(location, BlockDesc, node, tree);
	if (!b) {
		client->Unlock();
		return -1;
	}
	if (!(b->flags & BF_BUSY)) {
		client->Unlock();
		return -1;
	}
	range_t blockSize = b->busyHead.blockSize;
	void *allocArg = b->busyHead.allocArg;
	while (!LIST_ISEMPTY(b->busyHead.chain)) {
		BlockDesc *cb = LIST_FIRST(BlockDesc, busyChain.list, b->busyHead.chain);
		LIST_DELETE(busyChain.list, cb, b->busyHead.chain);
		assert(cb->flags & BF_BUSYCHAIN);
		cb->flags &= ~BF_BUSYCHAIN;
		AddFreeBlock(cb);
		MergeBlock(cb);
	}
	b->flags &= ~BF_BUSY;
	AddFreeBlock(b);
	MergeBlock(b);
	client->Unlock();
	client->Free(location, blockSize, allocArg);
	return 0;
}

template <typename range_t>
int
BuddyAllocator<range_t>::UnReserve(range_t location)
{
	assert(isInitialized);
	if (!isInitialized) {
		return -1;
	}
	KeepBlocks();
	client->Lock();
	BlockDesc *b = TREE_FIND(location, BlockDesc, node, tree);
	if (!b) {
		client->Unlock();
		return -1;
	}
	if (!(b->flags & BF_RESERVED)) {
		client->Unlock();
		return -1;
	}
	while (!LIST_ISEMPTY(b->busyHead.chain)) {
		BlockDesc *cb = LIST_FIRST(BlockDesc, busyChain.list, b->busyHead.chain);
		LIST_DELETE(busyChain.list, cb, b->busyHead.chain);
		assert(cb->flags & BF_RESCHAIN);
		cb->flags &= ~BF_RESCHAIN;
		AddFreeBlock(cb);
		MergeBlock(cb);
	}
	b->flags &= ~BF_RESERVED;
	AddFreeBlock(b);
	MergeBlock(b);
	client->Unlock();
	return 0;
}

template <typename range_t>
void
BuddyAllocator<range_t>::FreeTree()
{
	BlockDesc *d;

	if (freeBlocks) {
		client->mfree(freeBlocks);
		freeBlocks = 0;
	}
	while ((d = TREE_ROOT(BlockDesc, node, tree))) {
		TREE_DELETE(node, d, tree);
		client->FreeStruct(d, sizeof(BlockDesc));
	}
}

/* compile methods for supported base types */
__volatile void
BuddyCompilerStub()
{
#define REFTYPE(type) { \
	BuddyAllocator<type> __CONCAT(obj_,type)((BuddyAllocator<type>::BuddyClient *)0); \
	__CONCAT(obj_,type).Initialize(0,0,0,0); \
	__CONCAT(obj_,type).Reserve(0 ,0); \
	__CONCAT(obj_,type).Allocate(0 ,0); \
	__CONCAT(obj_,type).Free(0); \
	__CONCAT(obj_,type).Lookup(0); \
}
	REFTYPE(u8);
	REFTYPE(u16);
	REFTYPE(u32);
	REFTYPE(u64);
}
