/*
 * /kernel/sys/BuddyAllocator.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
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
		virtual int Allocate(range_t base, range_t size, void *arg = 0) = 0;
		virtual int Free(range_t base, range_t size, void *arg = 0) = 0;
		virtual void Lock() {}
		virtual void Unlock() {}
	};
private:
	enum {
		BPOOL_LOWWAT = 128,
		BPOOL_HIWAT = 256,
	};

	typedef struct {
		typename Tree<range_t>::TreeEntry node;
		u16 order;
		u16 flags;
		union {
			ListEntry list; /* either free blocks list or busy chain */
			struct {
				ListHead chain; /* busy chain head for head block */
				range_t blockSize;
				void *allocArg;
			} busyHead;
		};
	} BlockDesc;

	typedef enum {
		BF_FREE =			0x1,
		BF_RESERVED =		0x2,
		BF_BUSY =			0x4,
		BF_BUSYCHAIN =		0x8, /* 'list' is chain of busy blocks */
	} BlockFlags;

	int isInitialized;
	range_t base, size;
	u16 minOrder, maxOrder;
	BuddyClient *client;
	typename Tree<range_t>::TreeRoot tree;
	ListHead *freeBlocks;
	ListHead blocksPool;
	u32 numPool; /* real blocks number in pool */
	int numReqPool; /* requested blocks number for pool, request processing is pending */
	u32 numBlocks; /* total number of managed blocks (without pool) */

	void FreeTree();
	BlockDesc *AllocateBlock();
	void FreeBlock(BlockDesc *b);
	int KeepBlocks();
	BlockDesc *AddBlock(u16 order, range_t location);
	void DeleteBlock(BlockDesc *b);
	void AddFreeBlock(BlockDesc *b);
	void DeleteFreeBlock(BlockDesc *b);
	void SplitBlock(BlockDesc *b, u16 reqOrder);
	BlockDesc *MergeBlock(BlockDesc *b);
public:
	BuddyAllocator(BuddyClient *client) __noinline;
	virtual ~BuddyAllocator();

	virtual int Initialize(range_t base, range_t size, u16 minOrder, u16 maxOrder);
	virtual int Allocate(range_t size, range_t *location, void *arg = 0);
	virtual int Free(range_t location);
	virtual int Reserve(range_t location, range_t size);
};


#endif /* BUDDYALLOCATOR_H_ */