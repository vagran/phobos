/*
 * /kernel/kern/trap_hi.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

/* called before returning to ring 3 from interrupt/exception or system call */
ASMCALL void
OnUserRet()
{
	if (im) {
		im->Poll(1);
	}
	/* Check current process for faults, reschedule, etc. */
	if (pm) {
		pm->ValidateState();
	}
}

/* called before returning from trap */
ASMCALL void
OnTrapRet(Frame *frame)
{
	CPU *cpu = CPU::GetCurrent();

	if (GDT::GetRPL(frame->cs) == GDT::PL_USER ||
		(cpu && cpu->IsSetAST() && cpu->GetTrapNesting() == 1)) {
		if (cpu) {
			cpu->ClearAST();
			cpu->NestTrap(0);
		}
		OnUserRet();
	} else {
		if (cpu) {
			cpu->NestTrap(0);
		}
	}
}

ASMCALL int
OnTrap(Frame *frame)
{
	if (GDT::GetRPL(frame->cs) == GDT::PL_USER) {
		/* restore per-cpu reference segment selector from private TSS */
		CPU::RestoreSelector();
	}
	CPU *cpu = CPU::GetCurrent();
	if (cpu) {
		cpu->NestTrap();
	}
	if (frame->eflags & EFLAGS_IF) {
		sti();
	}
	if (idt) {
		return idt->HandleTrap(frame);
	}
	return 0;
}
