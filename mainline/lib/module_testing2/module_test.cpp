/*
 * /phobos/lib/module_testing_2/module_test_2.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <module_test.h>
#include "../module_testing/module_test_lib.h"

volatile u32 shlib2DATA = MT_DWORD_VALUE;

int
mtShlib2Test()
{
	if (shlibDATA != MT_DWORD_VALUE2) {
		return -1;
	}
	shlibDATA = MT_DWORD_VALUE;
	return 0;
}
