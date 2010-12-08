/*
 * /phobos/bin/module_testing_dyn/dyn_lib.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <module_test.h>
#include <module_test_lib.h>

volatile u32 execData;

class MTSharedLib : public MTest {
private:
public:
	MT_DECLARE(MTSharedLib);
};

MT_DESTR(MTSharedLib) {}

MT_DEFINE(MTSharedLib, "Shared library support")
{
	mt_assert(!shlibBSS);
	mt_assert(shlibDATA == MT_DWORD_VALUE);
	mt_assert(shlibConstDATA == MT_DWORD_VALUE2);

	mt_assert(!mtShlibGetBSS());
	shlibBSS = MT_DWORD_VALUE;
	mt_assert(mtShlibGetBSS() == MT_DWORD_VALUE);

	mt_assert(mtShlibGetDATA() == MT_DWORD_VALUE);
	shlibDATA = MT_DWORD_VALUE2;
	mt_assert(mtShlibGetDATA() == MT_DWORD_VALUE2);

	mt_assert(mtShlibGetConstDATA() == MT_DWORD_VALUE2);

	mtShlibModExec();
	mt_assert(execData == MT_DWORD_VALUE);

	mt_assert(!mtShlibTestOwnVars());

	mt_assert(!mtShlib2ExecFunc());

	mt_assert(!mtShlibTestWeakFunc());

	mt_assert(!mtShlibTestFuncPointer());
}

/* Called from shared library */
void
mtShlibModLib()
{
	shlibDATA = MT_DWORD_VALUE;
}

void
mtShlibWeakFunc3()
{
	shlibDATA = MT_DWORD_VALUE;
}
