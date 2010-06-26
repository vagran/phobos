/*
 * /phobos/kernel/kern/gm.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

GM *gm;

GM::GM()
{
}

int
GM::InitCPU()
{
	/* establish kernel entry point */
	wrmsr(MSR_SYSENTER_CS_MSR, gdt->GetSelector(GDT::SI_KCODE));
	return 0;
}
