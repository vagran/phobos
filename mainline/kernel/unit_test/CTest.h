/*
 * /kernel/unit_test/CTest.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef CTEST_H_
#define CTEST_H_

/*
 * Required standard function must be duplicated in framework because
 * we cannot include standard library files in unit tests sources.
 */

void
ut_printf(char *fmt,...);

void *
ut_malloc(int size, char *file, int line);

#define UTALLOC(type, num)	((type *)ut_malloc(sizeof(type) * num, __FILE__, __LINE__))

void
ut_free(void *p);

#define UTFREE(p)	ut_free(p)

char *
ut_strdup(char *p, char *file, int line);

#define UTSTRDUP(p)	ut_strdup(p, __FILE__, __LINE__)

int
ut_rand();

extern int UT_RAND_MAX;

void
ut_memset(void *p, int fill, int size);

void
ut_abort();

class CTest {
private:
	CTest *next;
	char *name, *desc;
public:
	CTest(char *name, char *desc);
	virtual ~CTest();

	static int ListTests();
	static int RunTests();
	static int RunTest(int idx);

	virtual int Run() = 0;
};

extern CTest *ut_testlist;

#define MAKETEST(className, name, desc) static className _ut_test_ ## className (name, desc);

#endif /* CTEST_H_ */
