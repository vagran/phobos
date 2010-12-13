/*
 * /phobos/bin/module_testing/sl_rtload/sl_rtload.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <sys.h>
phbSource("$Id$");

#include <module_test.h>
#include <module_test_lib.h>

volatile u32 execData;
volatile u32 constrCalled, destrCalled;

static RTLinker *linker;

class MTSharedLib : public MTest {
private:
public:
	MT_DECLARE(MTSharedLib);
};

MT_DESTR(MTSharedLib) {}

#define MT_DEFVAR(name) \
	u32 *__CONCAT(_, name) = (u32 *)linker->GetSymbolAddress(__STR(name)); \
	mt_assert(__CONCAT(_, name));

typedef u32 (*MTTestFunc)();
#define MT_DEFFUNC(name) \
	MTTestFunc __CONCAT(_, name) = (MTTestFunc)linker->GetSymbolAddress(__STR(name)); \
	mt_assert(__CONCAT(_, name));

MT_DEFINE(MTSharedLib, "Shared library run-time loading support")
{
	RTLinker::DSOHandle dso = GetDSO(&linker);
	mt_assert(linker);
	mt_assert(dso);
	RTLinker::DSOHandle loadedDso = linker->LoadLibrary("libmodule_testing.sl", dso);
	mt_assert(loadedDso);

	mt_assert(linker->GetSymbolAddress("mtShlib2Test"));

	MT_DEFVAR(shlibBSS);
	MT_DEFVAR(shlibDATA);
	MT_DEFVAR(shlibConstDATA);
	MT_DEFVAR(shlibObjCurOrder);
	MT_DEFVAR(shlibObjError);

	MT_DEFFUNC(mtShlibGetBSS);
	MT_DEFFUNC(mtShlibGetDATA);
	MT_DEFFUNC(mtShlibGetConstDATA);
	MT_DEFFUNC(mtShlibModExec);
	MT_DEFFUNC(mtShlibTestOwnVars);
	MT_DEFFUNC(mtShlib2ExecFunc);
	MT_DEFFUNC(mtShlibTestWeakFunc);
	MT_DEFFUNC(mtShlibTestFuncPointer);
	MT_DEFFUNC(mtShlibTestSecLib);

	mt_assert(!*_shlibBSS);
	mt_assert(*_shlibDATA == MT_DWORD_VALUE);
	mt_assert(*_shlibConstDATA == MT_DWORD_VALUE2);

	mt_assert(!_mtShlibGetBSS());
	*_shlibBSS = MT_DWORD_VALUE;
	mt_assert(_mtShlibGetBSS() == MT_DWORD_VALUE);

	mt_assert(_mtShlibGetDATA() == MT_DWORD_VALUE);
	*_shlibDATA = MT_DWORD_VALUE2;
	mt_assert(_mtShlibGetDATA() == MT_DWORD_VALUE2);

	mt_assert(_mtShlibGetConstDATA() == MT_DWORD_VALUE2);

	_mtShlibModExec();
	mt_assert(execData == MT_DWORD_VALUE);

	mt_assert(!_mtShlibTestOwnVars());

	mt_assert(!_mtShlib2ExecFunc());

	mt_assert(!_mtShlibTestWeakFunc());

	mt_assert(!_mtShlibTestFuncPointer());

	mt_assert(!_mtShlibTestSecLib());

	mt_assert(*_shlibObjCurOrder == 2);
	mt_assert(!*_shlibObjError);

	/* Test multiple references and unloading */

	RTLinker::DSOHandle loadedDso2 = linker->LoadLibrary("libmodule_testing.sl", dso);
	mt_assert(loadedDso2 == loadedDso);

	mt_assert(!linker->FreeLibrary(loadedDso));
	/* Should still persist */
	mt_assert(linker->GetSymbolAddress("mtShlib2Test"));
	mt_assert(linker->GetSymbolAddress("shlibBSS"));
	mt_assert(*_shlibObjCurOrder == 2);

	mt_assert(!linker->FreeLibrary(loadedDso));
	mt_assert(!*_shlibObjCurOrder);
	mt_assert(!*_shlibObjError);

	mt_assert(!linker->GetSymbolAddress("mtShlib2Test"));
	mt_assert(!linker->GetSymbolAddress("shlibBSS"));
}

/* Called from shared library */
ASMCALL void
mtShlibModLib()
{
	MT_DEFVAR(shlibDATA);
	*_shlibDATA = MT_DWORD_VALUE;
}

ASMCALL void
mtShlibWeakFunc3()
{
	MT_DEFVAR(shlibDATA);
	*_shlibDATA = MT_DWORD_VALUE;
}
