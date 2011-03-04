/*
 * /phobos/lib/module_testing/module_test_lib.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#ifndef MODULE_TEST_LIB_H_
#define MODULE_TEST_LIB_H_
#include <sys.h>
phbSource("$Id$");

#include <module_test.h>

extern volatile u32 shlibBSS, shlibDATA, shlibBSS2, shlibDATA2;
extern const u32 shlibConstDATA;
extern volatile u32 execData; /* Defined in executable */
extern volatile u32 constrCalled, destrCalled;

extern volatile u32 shlib2DATA;

ASMCALL u32 mtShlibGetDATA();
ASMCALL u32 mtShlibGetBSS();
ASMCALL u32 mtShlibGetConstDATA();

ASMCALL void mtShlibModExec();

ASMCALL void mtShlibModLib(); /* Defined in executable */

ASMCALL int mtShlibTestOwnVars();
ASMCALL int mtShlib2ExecFunc();
ASMCALL int mtShlibTestWeakFunc();
ASMCALL int mtShlibTestFuncPointer();
ASMCALL int mtShlib2Test();
ASMCALL int mtShlibTestSecLib();
ASMCALL RTLinker::DSOHandle mtShlibGetDSOHandle();
ASMCALL RTLinker::DSOHandle mtShlibGetDSOHandle2();

extern volatile int shlibObjCurOrder, shlibObjError;

class ShlibObj {
public:
	volatile int order, *curOrder, *error;
	ShlibObj(int order, volatile int *curOrder, volatile int *error) {
		this->order = order;
		this->curOrder = curOrder;
		this->error = error;
		if (*curOrder != order) {
			(*error)++;
		}
		(*curOrder)++;
	}

	~ShlibObj() {
		(*curOrder)--;
		if (*curOrder != order) {
			(*error)++;
		}
	}
};
#endif /* MODULE_TEST_LIB_H_ */
