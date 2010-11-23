/*
 * /kernel/kern/trap_hi.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
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
	int userMode = GDT::GetRPL(frame->cs) == GDT::PL_USER;
	if (userMode) {
		/* restore per-cpu reference segment selector from private TSS */
		CPU::RestoreSelector();
	}
	CPU *cpu = CPU::GetCurrent();
	if (cpu) {
		cpu->NestTrap();
	}
	PM::Thread *thrd = PM::Thread::GetCurrent();
	if (userMode && thrd) {
		thrd->SetTrapFrame(frame);
	}
	if (frame->eflags & EFLAGS_IF) {
		if (cpu && cpu->GetTrapNesting() < CPU::MAX_TRAP_NESTING) {
			sti();
		}
	}
	if (idt) {
		return idt->HandleTrap(frame);
	}
	if (userMode && thrd) {
		thrd->SetTrapFrame(0);
	}
	return 0;
}
