/*
 * /kernel/unit_test/main.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

/*
 * main.cpp - the only file which is compiled with native build system headers.
 * All the rest files compiled within PhobOS build environment.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>

#include "CTest.h"

CTest *ut_testlist;

CTest::CTest(const char *name, const char *desc)
{
	if (ut_testlist) {
		next = ut_testlist;
	} else {
		next = 0;
	}
	ut_testlist = this;
	this->name = UTSTRDUP(name);
	this->desc = UTSTRDUP(desc);
}

CTest::~CTest()
{
	if (name) {
		UTFREE(name);
	}
	if (desc) {
		UTFREE(desc);
	}
}

int
CTest::ListTests()
{
	CTest *p = ut_testlist;
	int i = 0;
	while (p) {
		ut_printf("%d. %s (%s)\n", i + 1, p->name, p->desc);
		i++;
		p = p->next;
	}
	return 0;
}

int
CTest::RunTests()
{
	return RunTest(-1);
}

int
CTest::RunTest(int idx)
{
	int failed = 0, run = 0;
	CTest *p = ut_testlist;
	int i = 0;
	while (p) {
		if ((idx != -1) && (i != idx)) {
			i++;
			p = p->next;
			continue;
		}
		ut_printf("========================================\n"
			"Running test %d: %s (%s)\n", i + 1, p->name, p->desc);
		run++;

		int rc = p->Run();
		if (rc) {
			ut_printf("Test failed. code = %d\n", rc);
			failed++;
		} else {
			ut_printf("Test successfully passed\n");
		}
		i++;
		p = p->next;
	}
	ut_printf("Total tests: %d, run: %d, failed: %d\n", i, run, failed);
	return failed != 0;
}

/* standard functions implementations */

void
__assert(const char *file, unsigned long line, const char *cond)
{
	printf("Assert failed at %s:%ld: '%s'\n", file, line, cond);
	ut_abort();
}

void
ut_abort()
{
	abort();
}

void
ut_printf(const char *fmt,...)
{
	va_list va;
	va_start(va, fmt);
	vprintf(fmt, va);
	va_end(va);
}

void *
ut_malloc(int size, const char *file, int line)
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

char *
ut_strdup(const char *p, const char *file, const int line)
{
	char *dup = (char *)ut_malloc(strlen(p) + 1, file, line);
	strcpy(dup, p);
	return dup;
}

int
ut_rand()
{
	return rand();
}

int UT_RAND_MAX = RAND_MAX;

void
ut_memset(void *p, int fill, int size)
{
	memset(p, fill, size);
}

void *
operator new(size_t size)
{
	return UTALLOC(char, size);
}

void
operator delete(void *p)
{
	UTFREE(p);
}

void *
operator new[](size_t size)
{
	return UTALLOC(char, size);
}

void
operator delete[](void *p)
{
	UTFREE(p);
}

extern "C" void
__cxa_pure_virtual()
{
    ut_printf("Fatal error: Pure virtual function called\n");
    exit(1);
}


void
usage()
{
	printf(
		"Usage:\n"
		"utrun <options>\n"
		"Available options:\n"
		"-l			list all tests\n"
		"-n idx		run specified test\n"
	);
}

int
main(int argc, char *argv[])
{
	int ch;
	int run_idx = -1;

	ut_printf("PhobOS unit testing procedure started\n");
	srand(time(0));
	while ((ch = getopt(argc, argv, "ln:")) != -1) {
		switch (ch) {
		case 'l':
			return CTest::ListTests();
		case 'n':
			if (!sscanf(optarg, "%d", &run_idx)) {
				ut_printf("Invalid decimal value: '%s'", optarg);
				exit(1);
			}
			run_idx--;
			break;
		case '?':
			usage();
			exit(1);
		}
	}
	return CTest::RunTest(run_idx);
}
