/*
 * /kernel/kern/trap_hi.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <kdb/debugger.h>

int debugFaults = 1;
int debugPanics = 1;

ASMCALL void
OnUserRet(u32 idx, Frame *frame)
{

}

void
panic(const char *fmt,...)
{
	va_list va;
	va_start(va, fmt);
	printf("panic: ");
	vprintf(fmt, va);
	printf("\n");
	if (debugPanics) {
		if (sysDebugger) {
			sysDebugger->Break();
		}
	}
	cli();
	hlt();
	while (1);
}

ASMCALL int
OnTrap(u32 idx, void *arg, Frame *frame)
{
	if (debugFaults && idx != IDT::ST_DEBUG && idx != IDT::ST_BREAKPOINT) {
		if (sysDebugger) {
			sysDebugger->Trap((IDT::SysTraps)idx, frame);
		}
	}
	return 0;
}

static int
UnhandledTrap(u32 idx, void *arg, Frame *frame)
{
	u32 esp;
	if (((SDT::SegSelector *)(void *)&frame->cs)->rpl) {
		esp = frame->esp;
	} else {
		esp = (u32)&frame->esp;
	}
	panic("Unhandled trap\nidx = %lx, code = %lu, eip = 0x%08lx, eflags = 0x%08lx\n"
		"eax = 0x%08lx, ebx = 0x%08lx, ecx = 0x%08lx, edx = 0x%08lx\n"
		"esi = 0x%08lx, edi = 0x%08lx, ebp = 0x%08lx, esp = 0x%08lx",
		idx, frame->u.errorcode, frame->eip, frame->eflags,
		frame->eax, frame->ebx, frame->ecx, frame->edx,
		frame->esi, frame->edi, frame->ebp, esp);
	return 0;
}

void
IDT::InitHandlers()
{
	RegisterUTHandler(UnhandledTrap);
}
