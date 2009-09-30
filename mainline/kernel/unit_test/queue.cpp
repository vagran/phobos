/*
 * /kernel/unit_test/queue.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
#include "CTest.h"

class QueueTest : public CTest {
private:


public:
	QueueTest(char *name, char *desc);
	virtual int Run();
};

QueueTest::QueueTest(char *name, char *desc) : CTest(name, desc)
{

}

int QueueTest::Run()
{

	return 0;
}

MAKETEST(QueueTest, "Linked lists", "Linked lists verification test");
