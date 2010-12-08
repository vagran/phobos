/*
 * /phobos/lib/module_testing/module_test_lib.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#ifndef MODULE_TEST_LIB_H_
#define MODULE_TEST_LIB_H_
#include <sys.h>
phbSource("$Id$");

#include <module_test.h>

extern volatile u32 shlibBSS, shlibDATA, shlibBSS2, shlibDATA2;
extern const u32 shlibConstDATA;
extern volatile u32 execData; /* Defined in executable */

u32 mtShlibGetDATA();
u32 mtShlibGetBSS();
u32 mtShlibGetConstDATA();

void mtShlibModExec();

void mtShlibModLib(); /* Defined in executable */

int mtShlibTestOwnVars();
int mtShlib2ExecFunc();
int mtShlibTestWeakFunc();
int mtShlibTestFuncPointer();

#endif /* MODULE_TEST_LIB_H_ */
