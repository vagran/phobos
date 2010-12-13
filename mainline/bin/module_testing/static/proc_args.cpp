/*
 * /phobos/bin/module_testing/static/proc_args.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <module_test.h>

class MTProcArgs : public MTest {
private:
public:
	MT_DECLARE(MTProcArgs);
};

MT_DESTR(MTProcArgs) {}

MT_DEFINE(MTProcArgs, "Process arguments")
{
	CString args = GETSTR(uLib->GetProcess(), GProcess::GetArgs);
	int idx = 0;
	CString s;
	mt_assert(args.GetToken(s, idx, &idx));
	/* The first token should be total number of the rest arguments */
	int num;
	mt_assert(s.Scanf("%d", &num));
	mt_assert(num);
	/* Verify the reset arguments are mt_arg0 mt_arg1 ... */
	for (int argIdx = 0; argIdx < num; argIdx++) {
		mt_assert(args.GetToken(s, idx, &idx));
		int i;
		mt_assert(s.Scanf("mt_arg%d", &i));
		mt_assert(i == argIdx);
	}
	/* Nothing should left in arguments string */
	mt_assert(idx == args.GetLength());
}
