/*
 * /phobos/kernel/unit_test/lock.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include "CTest.h"

class SyncTest : public CTest {
public:
	SyncTest(const char *name, const char *desc);
	virtual int Run();
};

MAKETEST(SyncTest, "System synchronization primitives",
	"System synchronization primitives verification test");

SyncTest::SyncTest(const char *name, const char *desc) : CTest(name, desc)
{
}

int
SyncTest::Run()
{
	AtomicInt<u32> i1;
	AtomicInt<u32> i2(237);

	if (i1 != 0) {
		ut_printf("i1 != 0\n");
		return -1;
	}

	if ((i1 = 237) != 237) {
		ut_printf("(i1 = 237) != 237\n");
		return -1;
	}

	if (i1 != 237) {
		ut_printf("i1 != 237\n");
		return -1;
	}

	if (i2 != 237) {
		ut_printf("i2 != 237");
		return -1;
	}

	++i2;
	if (i2 != 238) {
		ut_printf("++i2 != 238");
		return -1;
	}

	if (!--i2) {
		ut_printf("!--i2");
		return -1;
	}
	return 0;
}
