/*
 * /kernel/unit_test/slab.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
#include "CTest.h"

#include <SlabAllocator.h>

class SlabTest : public CTest {
public:
	SlabTest(char *name, char *desc);
	virtual int Run();
};

SlabTest::SlabTest(char *name, char *desc) : CTest(name, desc)
{
}

int
SlabTest::Run()
{
	return 0;
}

MAKETEST(SlabTest, "Slab allocator", "Slab allocator verification test");
