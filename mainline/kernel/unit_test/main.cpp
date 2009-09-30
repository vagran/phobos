/*
 * /kernel/unit_test/main.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "CTest.h"

CTest *ut_testlist;

CTest::CTest(char *name, char *desc)
{
	if (ut_testlist) {
		next = ut_testlist;
	} else {
		next = 0;
	}
	ut_testlist = this;
}

void
ut_printf(char *fmt,...)
{
	va_list va;
	va_start(va, fmt);
	vprintf(fmt, va);
	va_end(va);
}

void *
ut_malloc(size_t size, char *file, int line)
{
	void *p = malloc(size);
	if (!p) {
		ut_printf("Failed to allocate %d bytes (%s:%d)\n", size, file, line);
		exit(1);
	}
	return p;
}

void
ut_free(void *p)
{
	free(p);
}

extern "C" void
__cxa_pure_virtual()
{
    ut_printf("Fatal error: Pure virtual function called\n");
    exit(1);
}

int
main(int argc, char *argv[])
{
	ut_printf("PhobOS unit testing procedure started\n");

	return 0;
}
