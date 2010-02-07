/*
 * /kernel/kern/trap_hi.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <kdb/debugger.h>

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
	if (idt) {
		return idt->HandleTrap(frame);
	}
	return 0;
}

int
UnhandledTrap(Frame *frame, void *arg)
{
	u32 esp;
	if (((SDT::SegSelector *)(void *)&frame->cs)->rpl) {
		esp = frame->esp;
	} else {
		esp = (u32)&frame->esp;
	}
	if (Debugger::debugFaults && sysDebugger) {
		sysDebugger->Trap(frame);
	}
	panic("Unhandled trap\nidx = %lx (%s), code = %lu, eip = 0x%08lx, eflags = 0x%08lx\n"
		"eax = 0x%08lx, ebx = 0x%08lx, ecx = 0x%08lx, edx = 0x%08lx\n"
		"esi = 0x%08lx, edi = 0x%08lx, ebp = 0x%08lx, esp = 0x%08lx",
		frame->vectorIdx, IDT::StrTrap((IDT::SysTraps)frame->vectorIdx),
		frame->code, frame->eip, frame->eflags,
		frame->eax, frame->ebx, frame->ecx, frame->edx,
		frame->esi, frame->edi, frame->ebp, esp);
	return 0;
}
