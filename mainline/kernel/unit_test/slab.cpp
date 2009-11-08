/*
 * /kernel/unit_test/slab.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
#include "CTest.h"

#include <SlabAllocator.h>

class SlabTest : public CTest {
private:
	class Client : public SlabAllocator::SlabClient {
	private:
		SlabTest *parent;
	public:
		Client (SlabTest *parent) {this->parent = parent;}

		virtual void *malloc(u32 size) {
			Block *b = UTALLOC(Block, 1);
			b->p = UTALLOC(u8, size);
			LIST_ADD(list, b, parent->blocks);
			ut_printf("Block requested: %d bytes at %08x\n", size, b->p);
			return b->p;
		}

		virtual void mfree(void *p) {
			Block *b;
			LIST_FOREACH(Block, list, b, parent->blocks) {
				if (b->p == p) {
					ut_printf("Block freed: %08x\n", p);
					LIST_DELETE(list, b, parent->blocks);
					UTFREE(p);
					UTFREE(b);
					return;
				}
			}
			ut_printf("ERROR: Block %08x was not allocated\n");
			parent->error = 1;
		}

		virtual void FreeInitialPool(void *p, u32 size) {UTFREE(p);}
	};

	typedef struct {
		ListEntry list;
		void *p;
	} Block;

	ListHead blocks;
	int error;

	int _Run();
public:
	SlabTest(const char *name, const char *desc);
	virtual int Run();
};

SlabTest::SlabTest(const char *name, const char *desc) : CTest(name, desc)
{
	LIST_INIT(blocks);
	error = 0;
}

int
SlabTest::_Run()
{
	Client client(this);
	const u32 POOL_SIZE = 16384;
	const u32 INITIAL_ALLOCS = 16;
	const u32 BULK_ALLOCS = 768;
	u8 *pool = UTALLOC(u8, POOL_SIZE);
	SlabAllocator alloc(&client, pool, POOL_SIZE);

	typedef struct
	{
		ListEntry list;
		u32 size;
		u32 cs;
		u8 data[];
	} DataItem;
	DataItem *d;

	ListHead blocks1, blocks2;

	LIST_INIT(blocks1);
	LIST_INIT(blocks2);

	u32 total_mem = 0;
	ut_printf("Making initial allocation...\n");

	u32 bsize = 16;
	for (u32 i = 0; i < INITIAL_ALLOCS; i++) {
		DataItem *p = (DataItem *)alloc.malloc(bsize);
		if (!p) {
			ut_printf("Allocation failed for bsize = %d, i = %d\n", bsize, i);
			return -1;
		}
		total_mem += bsize;
		LIST_ADD(list, p, blocks1);
		p->size = bsize;
		p->cs = 0;
		for (u32 j = 0; j < bsize - OFFSETOF(DataItem, data); j++) {
			u8 b = (u8)ut_rand();
			p->data[j] = b;
			p->cs += b;
		}
	}

	bsize = 48;
	for (u32 i = 0; i < INITIAL_ALLOCS; i++) {
		DataItem *p = (DataItem *)alloc.malloc(bsize);
		if (!p) {
			ut_printf("Allocation failed for bsize = %d, i = %d\n", bsize, i);
			return -1;
		}
		total_mem += bsize;
		LIST_ADD(list, p, blocks2);
		p->size = bsize;
		p->cs = 0;
		for (u32 j = 0; j < bsize - OFFSETOF(DataItem, data); j++) {
			u8 b = (u8)ut_rand();
			p->data[j] = b;
			p->cs += b;
		}
	}

	ut_printf("Making bulk allocation...\n");
	bsize = 16;
	for (u32 i = 0; i < BULK_ALLOCS; i++) {
		DataItem *p = (DataItem *)alloc.malloc(bsize);
		if (!p) {
			ut_printf("Allocation failed for bsize = %d, i = %d\n", bsize, i);
			return -1;
		}
		total_mem += bsize;
		LIST_ADD(list, p, blocks1);
		p->size = bsize;
		p->cs = 0;
		for (u32 j = 0; j < bsize - OFFSETOF(DataItem, data); j++) {
			u8 b = (u8)ut_rand();
			p->data[j] = b;
			p->cs += b;
		}
	}
	bsize = 48;
	for (u32 i = 0; i < BULK_ALLOCS; i++) {
		DataItem *p = (DataItem *)alloc.malloc(bsize);
		if (!p) {
			ut_printf("Allocation failed for bsize = %d, i = %d\n", bsize, i);
			return -1;
		}
		total_mem += bsize;
		LIST_ADD(list, p, blocks1);
		p->size = bsize;
		p->cs = 0;
		for (u32 j = 0; j < bsize - OFFSETOF(DataItem, data); j++) {
			u8 b = (u8)ut_rand();
			p->data[j] = b;
			p->cs += b;
		}
	}

	ut_printf("Freeing blocks...\n");
	while ((d = LIST_FIRST(DataItem, list, blocks1))) {
		LIST_DELETE(list, d, blocks1);
		alloc.mfree(d);
	}
	while ((d = LIST_FIRST(DataItem, list, blocks2))) {
		LIST_DELETE(list, d, blocks2);
		alloc.mfree(d);
	}

	ut_printf("Making allocations second pass...\n");
	bsize = 16;
	for (u32 i = 0; i < BULK_ALLOCS; i++) {
		DataItem *p = (DataItem *)alloc.malloc(bsize);
		if (!p) {
			ut_printf("Allocation failed for bsize = %d, i = %d\n", bsize, i);
			return -1;
		}
		total_mem += bsize;
		LIST_ADD(list, p, blocks1);
		p->size = bsize;
		p->cs = 0;
		for (u32 j = 0; j < bsize - OFFSETOF(DataItem, data); j++) {
			u8 b = (u8)ut_rand();
			p->data[j] = b;
			p->cs += b;
		}
	}

	bsize = 48;
	for (u32 i = 0; i < BULK_ALLOCS; i++) {
		DataItem *p = (DataItem *)alloc.malloc(bsize);
		if (!p) {
			ut_printf("Allocation failed for bsize = %d, i = %d\n", bsize, i);
			return -1;
		}
		total_mem += bsize;
		LIST_ADD(list, p, blocks1);
		p->size = bsize;
		p->cs = 0;
		for (u32 j = 0; j < bsize - OFFSETOF(DataItem, data); j++) {
			u8 b = (u8)ut_rand();
			p->data[j] = b;
			p->cs += b;
		}
	}
	ut_printf("All done, calling destructors...\n");
	return 0;
}

int
SlabTest::Run()
{
	if (_Run()) {
		return -1;
	}
	if (error) {
		return -1;
	}
	if (!LIST_ISEMPTY(blocks)) {
		ut_printf("Unfreed blocks left:\n");
		Block *b;
		while ((b = LIST_FIRST(Block, list, blocks))) {
			ut_printf("p = %08x\n", b->p);
			LIST_DELETE(list, b, blocks);
			UTFREE(b->p);
			UTFREE(b);
		}
		return -1;
	}
	return 0;
}

MAKETEST(SlabTest, "Slab allocator", "Slab allocator verification test");
