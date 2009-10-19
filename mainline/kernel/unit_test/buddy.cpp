/*
 * /kernel/unit_test/buddy.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
#include "CTest.h"

#include <BuddyAllocator.h>

class BuddyClient : public BuddyAllocator<u32>::BuddyClient {
private:

public:
	BuddyClient();

	virtual int Allocate(u32 base, u32 size);
	virtual int Free(u32 base, u32 size);
	virtual void *malloc(u32 size);
	virtual void mfree(void *p);
};

void *
BuddyClient::malloc(u32 size)
{
	return UTALLOC(u8, size);
}

void
BuddyClient::mfree(void *p)
{
	UTFREE(p);
}

BuddyClient::BuddyClient()
{

}

int
BuddyClient::Allocate(u32 base, u32 size)
{
	return 0;
}

int
BuddyClient::Free(u32 base, u32 size)
{
	return 0;
}

class BuddyTest : public CTest {
public:
	BuddyTest(char *name, char *desc);
	virtual int Run();
};

BuddyTest::BuddyTest(char *name, char *desc) : CTest(name, desc)
{
}

int
BuddyTest::Run()
{
	const u32 size = (20 << 20) + 0x111111;
	u8 *mem = UTALLOC(u8, size);
	if (!mem) {
		ut_printf("Memory allocation failed\n");
		return -1;
	}

	ut_printf("0x%08x bytes of memory allocated\n", size);
	BuddyClient client;
	BuddyAllocator<u32> alloc(&client);
	ut_printf("Initializing allocator...");
	if (alloc.Initialize((u32)mem, size, 4, 20)) {
		ut_printf("failed\n");
		return -1;
	}
	ut_printf("OK\n");

	typedef struct {
		ListEntry list;
		u32 size;
		u32 cs;
		u8 data[];
	} DataItem;

	u32 total_mem = 0;
	u32 num_blocks = 0;
	u32 failCount = 0;
	ListHead head;
	LIST_INIT(head);

	ut_printf("Starting allocations, total memory in pool: 0x%08x bytes\n", size);
	while (1) {
		u32 bsize = OFFSETOF(DataItem, data) + (u64)ut_rand() * (1 << 16) / UT_RAND_MAX;
		DataItem *p;
		if (alloc.Allocate(bsize, (u32 *)&p)) {
			failCount++;
			ut_printf("Allocation fail %d: total = 0x%08x in %d blocks, requested 0x%08x\n",
				failCount, total_mem, num_blocks, bsize);
			if (failCount < 16) {
				continue;
			} else {
				break;
			}
		}
		num_blocks++;
		total_mem += bsize;
		LIST_ADD(list, p, head);
		p->size = bsize;
		p->cs = 0;
		for (u32 i = 0; i < bsize - OFFSETOF(DataItem, data); i++) {
			u8 b = (u8)ut_rand();
			p->data[i] = b;
			p->cs += b;
		}
	}

	ut_printf("Verifying data integrity...\n");
	DataItem *p;
	failCount = 0;
	LIST_FOREACH(DataItem, list, p, head) {
		u32 cs = 0;
		for (u32 i = 0; i < p->size - OFFSETOF(DataItem, data); i++) {
			cs += p->data[i];
		}
		if (cs != p->cs) {
			failCount++;
		}
	}
	ut_printf("%d blocks failed\n", failCount);
	if (failCount) {
		UTFREE(mem);
		return -1;
	}

	/* free half of the blocks and repeat allocations */
	u32 numToFree = num_blocks / 2;
	ut_printf("Freeing half of blocks...\n");
	failCount = 0;
	while (numToFree) {
		p = LIST_FIRST(DataItem, list, head);
		LIST_DELETE(list, p, head);
		total_mem -= p->size;
		num_blocks--;
		numToFree--;
		if (alloc.Free((u32)p)) {
			failCount++;
		}
	}
	ut_printf("%d blocks failed\n", failCount);
	if (failCount) {
		UTFREE(mem);
		return -1;
	}

	ut_printf("Starting allocations, currently 0x%08x/0x%08x bytes in %d blocks\n",
		total_mem, size, num_blocks);
	while (1) {
		u32 bsize = OFFSETOF(DataItem, data) + (u64)ut_rand() * (1 << 16) / UT_RAND_MAX;
		DataItem *p;
		if (alloc.Allocate(bsize, (u32 *)&p)) {
			failCount++;
			ut_printf("Allocation fail %d: total = 0x%08x in %d blocks, requested 0x%08x\n",
				failCount, total_mem, num_blocks, bsize);
			if (failCount < 16) {
				continue;
			} else {
				break;
			}
		}
		num_blocks++;
		total_mem += bsize;
		LIST_ADD(list, p, head);
		p->size = bsize;
		p->cs = 0;
		for (u32 i = 0; i < bsize - OFFSETOF(DataItem, data); i++) {
			u8 b = (u8)ut_rand();
			p->data[i] = b;
			p->cs += b;
		}
	}

	ut_printf("Verifying data integrity...\n");
	failCount = 0;
	LIST_FOREACH(DataItem, list, p, head) {
		u32 cs = 0;
		for (u32 i = 0; i < p->size - OFFSETOF(DataItem, data); i++) {
			cs += p->data[i];
		}
		if (cs != p->cs) {
			failCount++;
		}
	}
	ut_printf("%d blocks failed\n", failCount);
	if (failCount) {
		UTFREE(mem);
		return -1;
	}

	UTFREE(mem);
	return 0;
}

MAKETEST(BuddyTest, "Buddy allocator", "Buddy allocator verification test");
