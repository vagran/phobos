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
	this->client = client;
	TREE_INIT(tree);
	freeBlocks = 0;
}

template <typename range_t>
BuddyAllocator<range_t>::~BuddyAllocator()
{
	FreeTree();
}

template <typename range_t>
int
BuddyAllocator<range_t>::Initialize(range_t base, range_t size, u16 minOrder, u16 maxOrder)
{
	if (maxOrder <= minOrder) {
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
		if (base + ((range_t)1 << minOrder) > end) {
			break;
		}

		/* find maximal order for current base */
		int order = minOrder + 1;

		while (1) {
			range_t mask = ((range_t)1 << order) - 1;
			if ((base & mask) || (base + mask + 1) > end) {
				order--;
				break;
			}
			if (order >= maxOrder) {
				break;
			}
			order++;
		}

		BlockDesc *b = AddBlock(order, base);
		AddFreeBlock(b);
		base += (range_t)1 << order;
	}

	return 0;
}

template <typename range_t>
void
BuddyAllocator<range_t>::AddFreeBlock(BlockDesc *b)
{
	assert(!(b->flags & BF_FREE));
	LIST_ADD(list, b, freeBlocks[b->order - minOrder]);
	b->flags |= BF_FREE;
}

template <typename range_t>
void
BuddyAllocator<range_t>::DeleteFreeBlock(BlockDesc *b)
{
	assert(b->flags & BF_FREE);
	LIST_DELETE(list, b, freeBlocks[b->order - minOrder]);
	b->flags &= ~BF_FREE;
}

template <typename range_t>
int
BuddyAllocator<range_t>::Allocate(range_t size, range_t *location)
{
	if (!size) {
		return -1;
	}
	range_t sz = size - 1;
	u16 reqOrder = 0;
	while (sz) {
		sz >>= 1;
		reqOrder++;
	}
	if (reqOrder < minOrder || reqOrder > maxOrder) {
		return -1;
	}

	BlockDesc *b = 0;
	for (u16 order = reqOrder; order <= maxOrder; order++) {
		ListHead &head = freeBlocks[order - minOrder];
		if (!LIST_ISEMPTY(head)) {
			b = LIST_FIRST(BlockDesc, list, head);
			DeleteFreeBlock(b);
			break;
		}
	}
	if (!b) {
		/* no free blocks */
		return -1;
	}
	if (b->order > reqOrder) {
		SplitBlock(b, reqOrder);
	}
	*location = b->node.key;
	client->Allocate(*location, (range_t)1 << reqOrder);
	return 0;
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
		BlockDesc *adjBlock;
		TREE_FIND(adjLoc, BlockDesc, node, adjBlock, tree);
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
	BlockDesc *b = (BlockDesc *)client->AllocateStruct(sizeof(BlockDesc));
	if (!b) {
		return 0;
	}
	b->order = order;
	b->flags = 0;
	b->node.key = location;
	TREE_ADD(node, b, tree);
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
	client->FreeStruct(b, sizeof(*b));
}

template <typename range_t>
int
BuddyAllocator<range_t>::Free(range_t location)
{
	BlockDesc *b;
	TREE_FIND(location, BlockDesc, node, b, tree);
	if (!b) {
		return -1;
	}
	if (b->flags & BF_FREE) {
		return -1;
	}
	client->Free(location, (range_t)1 << b->order);
	AddFreeBlock(b);
	MergeBlock(b);
	return 0;
}

template <typename range_t>
void
BuddyAllocator<range_t>::FreeTree()
{
	BlockDesc *d;

	if (freeBlocks) {
		client->FreeStruct(freeBlocks, (maxOrder - minOrder + 1) * sizeof(ListHead));
		freeBlocks = 0;
	}
	while ((d = TREE_ROOT(BlockDesc, node, tree))) {
		TREE_DELETE(node, d, tree);
		client->FreeStruct(d, sizeof(BlockDesc));
	}
}

/* compile methods for supported base types */
static void
CompilerStub()
{
#define REFTYPE(type) { \
	BuddyAllocator<type> __CONCAT(obj_,type)((BuddyAllocator<type>::BuddyClient *)0); \
	__CONCAT(obj_,type).Initialize(0,0,0,0); \
	__CONCAT(obj_,type).Allocate(0 ,0); \
	__CONCAT(obj_,type).Free(0); \
}
	REFTYPE(u8);
	REFTYPE(u16);
	REFTYPE(u32);
	REFTYPE(u64);
}

template <typename range_t>
void
BuddyAllocator<range_t>::CompilerStub()
{
	::CompilerStub();
}
