/*
 * /phobos/lib/module_testing/module_testing.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <module_test.h>
#include "module_test_lib.h"

volatile u32 shlibBSS;
volatile u32 shlibDATA = MT_DWORD_VALUE;
volatile u32 shlibBSS2;
volatile u32 shlibDATA2 = MT_DWORD_VALUE;
const u32 shlibConstDATA = MT_DWORD_VALUE2;

ASMCALL u32
mtShlibGetDATA()
{
	return shlibDATA;
}

ASMCALL u32
mtShlibGetConstDATA()
{
	return shlibConstDATA;
}


ASMCALL u32
mtShlibGetBSS()
{
	return shlibBSS;
}

ASMCALL void
mtShlibModExec()
{
	execData = MT_DWORD_VALUE;
}

ASMCALL int
mtShlib2ExecFunc()
{
	shlibDATA = 0;
	mtShlibModLib();
	return shlibDATA != MT_DWORD_VALUE;
}

ASMCALL int
mtShlibTestOwnVars()
{
	return shlibBSS2 || shlibDATA2 != MT_DWORD_VALUE;
}

ASMCALL RTLinker::DSOHandle
mtShlibGetDSOHandle()
{
	return GetDSO();
}

ASMCALL int
mtShlibTestSecLib()
{
	if (shlib2DATA != MT_DWORD_VALUE) {
		return -1;
	}
	shlibDATA = MT_DWORD_VALUE2;
	if (mtShlib2Test()) {
		return -1;
	}
	if (shlibDATA != MT_DWORD_VALUE) {
		return -1;
	}
	return 0;
}

/******************************************************************************/
/* Weak symbols linking */

ASMCALL void mtShlibWeakFunc1() __attribute__((weak)); /* Not defined */
ASMCALL void mtShlibWeakFunc2() __attribute__((weak)); /* Defined in library */
ASMCALL void mtShlibWeakFunc3() __attribute__((weak)); /* Defined in executable */

ASMCALL void
mtShlibWeakFunc2()
{
	shlibDATA = MT_DWORD_VALUE;
}

ASMCALL int
mtShlibTestWeakFunc()
{
	if (mtShlibWeakFunc1) {
		/* should not be defined */
		return -1;
	}
	shlibDATA = 0;
	if (mtShlibWeakFunc2) {
		mtShlibWeakFunc2();
	}
	if (shlibDATA != MT_DWORD_VALUE) {
		return -1;
	}
	shlibDATA = 0;
	if (mtShlibWeakFunc3) {
		mtShlibWeakFunc3();
	}
	return shlibDATA != MT_DWORD_VALUE;
}

/******************************************************************************/
/* Function pointers
 * Initial assignments will produce R_386_RELATIVE relocation entries.
 */

static u32
mtTestHandler()
{
	shlibDATA = MT_DWORD_VALUE;
	return MT_DWORD_VALUE2;
}

typedef u32 (*mtHandler_t)();
static volatile mtHandler_t mtHandler = mtTestHandler;
static const mtHandler_t mtHandler2 = mtTestHandler;

ASMCALL int
mtShlibTestFuncPointer()
{
	shlibDATA = 0;
	if (mtHandler() != MT_DWORD_VALUE2 || shlibDATA != MT_DWORD_VALUE) {
		return -1;
	}
	shlibDATA = 0;
	if (mtHandler2() != MT_DWORD_VALUE2 || shlibDATA != MT_DWORD_VALUE) {
		return -1;
	}
	return 0;
}

/******************************************************************************/
/* Global objects constructors and destructors */

volatile int shlibObjCurOrder, shlibObjError;

static ShlibObj shlibObj(1, &shlibObjCurOrder, &shlibObjError);
