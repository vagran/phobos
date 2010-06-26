/*
 * /kernel/kern/trap_hi.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

/* called before returning to ring 3 from interrupt/exception or system call */
ASMCALL void
OnUserRet(Frame *frame)
{
	if (im) {
		im->Poll(1);
	}
}

ASMCALL int
OnTrap(Frame *frame)
{
	if (GDT::GetRPL(frame->cs) == GDT::PL_USER) {
		/* restore per-cpu reference segment selector from private TSS */
		CPU::RestoreSelector();
	}
	if (idt) {
		return idt->HandleTrap(frame);
	}
	return 0;
}
