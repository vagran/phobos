/*
 * /kernel/init/main.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <boot.h>
#include <gcc.h>

MBInfo *pMBInfo;

static char copyright[]	=
	"******************************************************\n"
	"PhobOS operating system\n"
	"Written by Artemy Lebedev\n"
	"Copyright ©AST 2009\n";

int
Main(paddr_t firstAddr)
{
	/* setup basic memory management */
	mm::PreInitialize((vaddr_t)(firstAddr - LOAD_ADDRESS + KERNEL_ADDRESS));
	/* call constructors for all static objects */
	CXA::ConstructStaticObjects();
	/* create default system console */
	sysCons = (ConsoleDev *)devMan.CreateDevice("syscons");
	printf(copyright);

	hlt();
	return 0;
}
