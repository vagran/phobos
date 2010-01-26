/*
 * /kernel/kern/tm.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

TM *tm;

TM::TM()
{
	pit = (PIT *)devMan.CreateDevice("pit", 0);
	ensure(pit);
}
