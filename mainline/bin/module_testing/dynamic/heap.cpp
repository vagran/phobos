/*
 * /phobos/bin/module_testing/dynamic/heap.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <module_test.h>

class MTHeap : public MTest {
private:
public:
	MT_DECLARE(MTHeap);
};

MT_DESTR(MTHeap) {}

MT_DEFINE(MTHeap, "Process heap operations")
{
	const u32 bufSize = 1024 * 1024;
	GApp *app = uLib->GetApp();

	u32 *buf = (u32 *)app->AllocateHeap(bufSize);
	mt_assert(buf);
	for (u32 i = 0; i < bufSize / sizeof(u32); i++) {
		buf[i] = MT_DWORD_VALUE;
	}
	for (u32 i = 0; i < bufSize / sizeof(u32); i++) {
		mt_assert(buf[i] == MT_DWORD_VALUE);
	}
	mt_assert(app->GetHeapSize(buf) == roundup2(bufSize, PAGE_SIZE));
	mt_assert(!app->FreeHeap(buf));

	/* Verify zeroed pages */
	buf = (u32 *)app->AllocateHeap(bufSize, MM::PROT_READ | MM::PROT_WRITE, 0, 1);
	mt_assert(buf);
	for (u32 i = 0; i < bufSize / sizeof(u32); i++) {
		mt_assert(!buf[i]);
		buf[i] = MT_DWORD_VALUE;
	}
	for (u32 i = 0; i < bufSize / sizeof(u32); i++) {
		mt_assert(buf[i] == MT_DWORD_VALUE);
	}
	mt_assert(app->GetHeapSize(buf) == roundup2(bufSize, PAGE_SIZE));
	mt_assert(!app->FreeHeap(buf));
}
