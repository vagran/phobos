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

u32
mtShlibGetDATA()
{
	return shlibDATA;
}

u32
mtShlibGetConstDATA()
{
	return shlibConstDATA;
}


u32
mtShlibGetBSS()
{
	return shlibBSS;
}

void
mtShlibModExec()
{
	execData = MT_DWORD_VALUE;
}

int
mtShlib2ExecFunc()
{
	shlibDATA = 0;
	mtShlibModLib();
	return shlibDATA != MT_DWORD_VALUE;
}

int
mtShlibTestOwnVars()
{
	return shlibBSS2 || shlibDATA2 != MT_DWORD_VALUE;
}

int
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

extern void mtShlibWeakFunc1() __attribute__((weak)); /* Not defined */
extern void mtShlibWeakFunc2() __attribute__((weak)); /* Defined in library */
extern void mtShlibWeakFunc3() __attribute__((weak)); /* Defined in executable */

void
mtShlibWeakFunc2()
{
	shlibDATA = MT_DWORD_VALUE;
}

int
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

int
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

class ShlibObj {
public:
	ShlibObj() { constrCalled++; }
	~ShlibObj() { destrCalled++; }
};

static ShlibObj shlibObj;
