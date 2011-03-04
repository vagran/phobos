/*
 * /phobos/kernel/unit_test/sslab.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#include <sys.h>
phbSource("$Id$");

#include <sys.h>
#include "CTest.h"

#include <SSlabAllocator.h>

class SSlabTest : public CTest {
private:
	class Client : public SSlabAllocator::Client {
	private:
		SSlabTest *parent;
		int lock;
	public:
		Client (SSlabTest *parent) {this->parent = parent; lock = 0; }

		virtual void *malloc(u32 size) {
			Block *b = UTALLOC(Block, 1);
			b->p = UTALLOC(u8, size + PAGE_SIZE - 1);
			memset(b->p, 0xbb, size + PAGE_SIZE - 1);
			b->size = size;
			parent->total_size += size;
			LIST_ADD(list, b, parent->blocks);
			return (void *)roundup2((u32)b->p, PAGE_SIZE);
		}

		virtual void mfree(void *p) {
			Block *b;
			LIST_FOREACH(Block, list, b, parent->blocks) {
				if ((void *)roundup2((u32)b->p, PAGE_SIZE) == p) {
					LIST_DELETE(list, b, parent->blocks);
					parent->total_size -= b->size;
					UTFREE(b->p);
					UTFREE(b);
					return;
				}
			}
			ut_printf("ERROR: Block %08x was not allocated\n");
			parent->error = 1;
		}

		virtual void Lock() {
			if (lock) {
				ut_printf("ERROR: recursively acquired lock\n");
				parent->error = 1;
			} else {
				lock = 1;
			}
		}

		virtual void Unlock() {
			if (lock) {
				lock = 0;
			} else {
				ut_printf("ERROR: unlocking without lock acquired\n");
				parent->error = 1;
			}
		}
	};

	typedef struct {
		ListEntry list;
		void *p;
		u32 size;
	} Block;

	ListHead blocks;
	int error;
	u32 total_size;

	int _Run();
public:
	SSlabTest(const char *name, const char *desc);
	virtual int Run();
};

SSlabTest::SSlabTest(const char *name, const char *desc) : CTest(name, desc)
{
	LIST_INIT(blocks);
	error = 0;
	total_size = 0;
}

int
SSlabTest::_Run()
{
	Client client(this);
	const u32 INITIAL_ALLOCS = 65536;
	const u32 BULK_ALLOCS = 4 * 1024;
	SSlabAllocator alloc(&client);

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
		DataItem *p = (DataItem *)alloc.AllocateStruct(bsize);
		if (!p) {
			ut_printf("Allocation failed for bsize = %d, i = %d\n", bsize, i);
			return -1;
		}
		total_mem += bsize;
		LIST_ADD(list, p, blocks1);
		p->size = bsize;
		p->cs = 0;
		for (int j = 0; j < (int)(bsize - OFFSETOF(DataItem, data)); j++) {
			u8 b = 0xaa;//(u8)ut_rand();
			p->data[j] = b;
			p->cs += b;
		}
	}

	bsize = 48;
	for (u32 i = 0; i < INITIAL_ALLOCS; i++) {
		DataItem *p = (DataItem *)alloc.AllocateStruct(bsize);
		if (!p) {
			ut_printf("Allocation failed for bsize = %d, i = %d\n", bsize, i);
			return -1;
		}
		total_mem += bsize;
		LIST_ADD(list, p, blocks2);
		p->size = bsize;
		p->cs = 0;
		for (int j = 0; j < (int)(bsize - OFFSETOF(DataItem, data)); j++) {
			u8 b = 0xaa;//(u8)ut_rand();
			p->data[j] = b;
			p->cs += b;
		}
	}

	ut_printf("Memory utilization: %d of %d bytes (%.3g%%)\n",
		total_mem, total_size, (double)total_mem / total_size * 100.0);

	ut_printf("Making bulk allocation...\n");
	bsize = 16;
	for (u32 i = 0; i < BULK_ALLOCS; i++) {
		DataItem *p = (DataItem *)alloc.AllocateStruct(bsize);
		if (!p) {
			ut_printf("Allocation failed for bsize = %d, i = %d\n", bsize, i);
			return -1;
		}
		total_mem += bsize;
		LIST_ADD(list, p, blocks1);
		p->size = bsize;
		p->cs = 0;
		for (int j = 0; j < (int)(bsize - OFFSETOF(DataItem, data)); j++) {
			u8 b = 0xaa;//(u8)ut_rand();
			p->data[j] = b;
			p->cs += b;
		}
	}
	bsize = 48;
	for (u32 i = 0; i < BULK_ALLOCS; i++) {
		DataItem *p = (DataItem *)alloc.AllocateStruct(bsize);
		if (!p) {
			ut_printf("Allocation failed for bsize = %d, i = %d\n", bsize, i);
			return -1;
		}
		total_mem += bsize;
		LIST_ADD(list, p, blocks1);
		p->size = bsize;
		p->cs = 0;
		for (int j = 0; j < (int)(bsize - OFFSETOF(DataItem, data)); j++) {
			u8 b = 0xaa;//(u8)ut_rand();
			p->data[j] = b;
			p->cs += b;
		}
	}
	ut_printf("Memory utilization: %d of %d bytes (%.3g%%)\n",
		total_mem, total_size, (double)total_mem / total_size * 100.0);

	ut_printf("Random size allocations...\n");
	for (u32 i = 0; i < BULK_ALLOCS; i++) {
		bsize = OFFSETOF(DataItem, data) + (ut_rand() & 0x8fff);
		DataItem *p = (DataItem *)alloc.AllocateStruct(bsize);
		if (!p) {
			ut_printf("Allocation failed for bsize = %d, i = %d\n", bsize, i);
			return -1;
		}
		total_mem += bsize;
		LIST_ADD(list, p, blocks1);
		p->size = bsize;
		p->cs = 0;
		for (int j = 0; j < (int)(bsize - OFFSETOF(DataItem, data)); j++) {
			u8 b = 0xaa;//(u8)ut_rand();
			p->data[j] = b;
			p->cs += b;
		}
	}
	ut_printf("Memory utilization: %d of %d bytes (%.3g%%)\n",
		total_mem, total_size, (double)total_mem / total_size * 100.0);

	ut_printf("Freeing blocks...\n");
	while ((d = LIST_FIRST(DataItem, list, blocks1))) {
		LIST_DELETE(list, d, blocks1);
		total_mem -= d->size;
		alloc.FreeStruct(d, 0);
	}
	while ((d = LIST_FIRST(DataItem, list, blocks2))) {
		LIST_DELETE(list, d, blocks2);
		total_mem -= d->size;
		alloc.FreeStruct(d, 0);
	}
	assert(!total_mem);

	ut_printf("Making allocations second pass...\n");
	for (u32 i = 0; i < BULK_ALLOCS; i++) {
		bsize = OFFSETOF(DataItem, data) + (ut_rand() & 0x8fff);
		DataItem *p = (DataItem *)alloc.AllocateStruct(bsize);
		if (!p) {
			ut_printf("Allocation failed for bsize = %d, i = %d\n", bsize, i);
			return -1;
		}
		total_mem += bsize;
		LIST_ADD(list, p, blocks1);
		p->size = bsize;
		p->cs = 0;
		for (int j = 0; j < (int)(bsize - OFFSETOF(DataItem, data)); j++) {
			u8 b = 0xaa;//(u8)ut_rand();
			p->data[j] = b;
			p->cs += b;
		}
	}
	ut_printf("Memory utilization: %d of %d bytes (%.3g%%)\n",
		total_mem, total_size, (double)total_mem / total_size * 100.0);
	ut_printf("All done, calling destructors...\n");
	return 0;
}

int
SSlabTest::Run()
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

MAKETEST(SSlabTest, "Scalable slab allocator", "Scalable slab allocator verification test");
