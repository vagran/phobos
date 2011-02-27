/*
 * /phobos/lib/module_testing_2/module_test_2.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
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

RTLinker::DSOHandle
mtShlibGetDSOHandle2()
{
	return GetDSO();
}

/******************************************************************************/
/* Global objects constructors and destructors */

static ShlibObj shlibObj(0, &shlibObjCurOrder, &shlibObjError);
