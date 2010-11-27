/*
 * /phobos/bin/module_testing/module_test.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include "module_test.h"

/**************************************************/
/* Test static objects construction */

class TestObj {
public:
	volatile u32 testProp;
	static volatile u32 testStaticProp;

	TestObj();
};

volatile u32 TestObj::testStaticProp = MT_DWORD_VALUE;

TestObj::TestObj()
{
	testProp = MT_DWORD_VALUE;
}

volatile TestObj testStaticObj;

void
TestStaticObj()
{
	mt_assert(testStaticObj.testStaticProp == MT_DWORD_VALUE);
	mt_assert(testStaticObj.testProp == MT_DWORD_VALUE);
}

/**************************************************/

#define TEST_BSS_SIZE (1024 * 1024)
volatile u8 testBssArea[TEST_BSS_SIZE];

volatile u32 cowTestArea = MT_DWORD_VALUE;

void
TestDataSegments()
{
	/* Test BSS area initialization */
	for (size_t i = 0; i < sizeof(testBssArea); i++) {
		mt_assert(!testBssArea[i]);
	}
	testBssArea[0] = MT_BYTE_VALUE;
	mt_assert(testBssArea[0] == MT_BYTE_VALUE);

	/* Test copy-on-write functionality */
	mt_assert(cowTestArea == MT_DWORD_VALUE);
	cowTestArea = MT_DWORD_VALUE2;
	mt_assert(cowTestArea == MT_DWORD_VALUE2);
}

/**************************************************/

int
Main(GApp *app)
{
	/*
	 * Do basic low-level check here because we do not yet know if static
	 * objects constructors are functional.
	 */
	mt_assert(app);

	TestDataSegments();

	TestStaticObj();

	mtlog("Module testing successfully finished");
	return 0;
}
