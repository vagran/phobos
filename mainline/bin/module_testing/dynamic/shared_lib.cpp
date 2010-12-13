/*
 * /phobos/bin/module_testing/dynamic/dyn_lib.cpp
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
volatile u32 constrCalled, destrCalled;

#define NUM_DYN_ARGS	16

class MTSharedLib : public MTest {
private:
public:
	MT_DECLARE(MTSharedLib);
};

MT_DESTR(MTSharedLib) {}

MT_DEFINE(MTSharedLib, "Shared library support")
{
	RTLinker::DSOHandle dso1 = mtShlibGetDSOHandle();
	RTLinker::DSOHandle dso2 = mtShlibGetDSOHandle2();
	mt_assert(dso1 && dso2);
	mt_assert(dso1 != dso2);
	RTLinker::DSOHandle dso = GetDSO();
	mt_assert(dso);
	mt_assert(dso != dso1);
	mt_assert(dso != dso2);

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

	mt_assert(!mtShlibTestSecLib());

	mt_assert(shlibObjCurOrder == 2);
	mt_assert(!shlibObjError);

	/* Launch new process to test run-time loading */
	CString args;

	args.Format("%d", NUM_DYN_ARGS);
	for (int i = 0; i < NUM_DYN_ARGS; i++) {
		args.AppendFormat(" mt_arg%d", i);
	}
	GProcess *proc = uLib->GetApp()->CreateProcess("/bin/sl_rtload_test",
		"Shared libraries run-time loading module test", PM::DEF_PRIORITY, args);
	mt_assert(proc);
	proc->Release();
}

/* Called from shared library */
ASMCALL void
mtShlibModLib()
{
	shlibDATA = MT_DWORD_VALUE;
}

ASMCALL void
mtShlibWeakFunc3()
{
	shlibDATA = MT_DWORD_VALUE;
}
