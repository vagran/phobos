/*
 * /kernel/sys/BuddyAllocator.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef BUDDYALLOCATOR_H_
#define BUDDYALLOCATOR_H_
#include <sys.h>
phbSource("$Id$");

#include <MemAllocator.h>

/*
 * Buddy allocator is universal buddy space allocator.
 * Underlying system must provide client object of BuddyClient class.
 */

template <typename range_t>
class BuddyAllocator {
public:
	class BuddyClient : public MemAllocator {
	public:
		BuddyClient() {};

		virtual int Allocate(range_t base, range_t size) = 0;
		virtual int Free(range_t base, range_t size) = 0;
	};
private:
	typedef struct {
		typename Tree<range_t>::TreeEntry node;
		u16 order;
		u16 flags;
		ListEntry list;
	} BlockDesc;

	typedef enum {
		BF_FREE =		0x1,
	} BlockFlags;

	range_t base, size;
	u16 minOrder, maxOrder;
	BuddyClient *client;
	typename Tree<range_t>::TreeRoot tree;
	ListHead *freeBlocks;

	void CompilerStub();
	void FreeTree();
	BlockDesc *AddBlock(u16 order, range_t location);
	void DeleteBlock(BlockDesc *b);
	void AddFreeBlock(BlockDesc *b);
	void DeleteFreeBlock(BlockDesc *b);
	void SplitBlock(BlockDesc *b, u16 reqOrder);
	BlockDesc *MergeBlock(BlockDesc *b);
public:
	BuddyAllocator(BuddyClient *client);
	~BuddyAllocator();

	int Initialize(range_t base, range_t size, u16 minOrder, u16 maxOrder);
	int Allocate(range_t size, range_t *location);
	int Free(range_t location);
};


#endif /* BUDDYALLOCATOR_H_ */
