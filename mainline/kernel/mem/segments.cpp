/*
 * /kernel/mem/segments.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

GDT *gdt;
IDT *idt;

void
SDT::SetDescriptor(Descriptor *d, u32 base, u32 limit, u32 ring, u32 system, u32 type)
{
	if ((limit & 0xfff) != 0xfff || limit < 0x100000) {
		if (limit > 0xfffff) {
			panic("Specifying segment with 1-byte granularity and limit >1MB, "
				"limit = 0x%08lX", limit);
		}
		d->gran = 0;
	} else {
		limit = limit >> 12;
		d->gran = 1;
	}
	d->lolimit = limit & 0xffff;
	d->hilimit = (limit >> 16) & 0xf;

	d->lobase = base & 0xffffff;
	d->hibase = base >> 24;

	d->data = !system;
	d->dpl = ring;
	d->type = type;
	d->p = 1;
	d->def32 = 1;
	d->unused = 0;
}

void
SDT::SetGate(Gate *g, u32 offset, u16 selector, u32 type)
{
	g->data = 0;
	g->dpl = 0;
	g->p = 1;
	g->type = type;
	g->selector = selector;
	g->paramCount = 0;
	g->unused = 0;
	g->looffset = offset & 0xffff;
	g->hioffset = offset >> 16;
}

/*************************************************************/

GDT::GDT()
{
	ldt = (ldtTable *)MM::malloc(sizeof(ldtTable), sizeof(SDT::Descriptor));
	memset(ldt, 0 ,sizeof(*ldt));
	table = (Table *)MM::malloc(sizeof(Table), sizeof(SDT::Descriptor));
	memset(&table->null, 0, sizeof(table->null));
	SDT::SetDescriptor(&table->kcode, 0, 0xffffffff, 0, 0,
		SDT::DST_CODE | SDT::DST_C_READ);
	SDT::SetDescriptor(&table->kdata, 0, 0xffffffff, 0, 0,
			SDT::DST_DATA | SDT::DST_D_WRITE);
	SDT::SetDescriptor(&table->ucode, 0, 0xffffffff, 3, 0,
		SDT::DST_CODE | SDT::DST_C_READ);
	SDT::SetDescriptor(&table->udata, 0, 0xffffffff, 3, 0,
		SDT::DST_DATA | SDT::DST_D_WRITE);
	SDT::SetDescriptor(&table->ldt, (u32)ldt, sizeof(*ldt) - 1, 3, 1,
		SDT::SST_LDT);
	pd.base = (u32)table;
	pd.limit = sizeof(*table) - 1;
	__asm __volatile (
		"lgdt	%0\n"
		"pushl	%1\n"
		"pushl	$0f\n"
		"lret\n"
		"0: mov	%2, %%ss\n"
		"mov	%2, %%ds\n"
		"mov	%2, %%es\n"
		"mov	%2, %%fs\n"
		"mov	%2, %%gs\n"
		"lldt	%3\n"
		:
		:"m"(pd), "r"((u32)GetSelector(SI_KCODE, 0)), "r"((u32)GetSelector(SI_KDATA, 0)),
		 "r"(GetSelector(SI_LDT, 0))
		);
}

/*************************************************************/

#define DeclareTrap(idx)	ASMCALL void __CONCAT(TrapEntry,idx)()

#define SetTrap(idx) SDT::SetGate(&table[idx], (u32)__CONCAT(TrapEntry,idx), \
	GDT::GetSelector(GDT::SI_KCODE, 0), SDT::SST_TGT32)

DeclareTrap(0x00);
DeclareTrap(0x01);
DeclareTrap(0x02);
DeclareTrap(0x03);
DeclareTrap(0x04);
DeclareTrap(0x05);
DeclareTrap(0x06);
DeclareTrap(0x07);
DeclareTrap(0x08);
DeclareTrap(0x09);
DeclareTrap(0x0a);
DeclareTrap(0x0b);
DeclareTrap(0x0c);
DeclareTrap(0x0d);
DeclareTrap(0x0e);
DeclareTrap(0x0f);
DeclareTrap(0x10);
DeclareTrap(0x11);
DeclareTrap(0x12);
DeclareTrap(0x13);

IDT::IDT()
{
	memset(hdlArgs, 0, sizeof(hdlArgs));
	_trapHandlersTable = handlersTable;
	_trapHandlersArgs = hdlArgs;
	memset(handlersTable, 0, sizeof(handlersTable));
	InitHandlers();

	table = (SDT::Gate *)MM::malloc(256 * sizeof(*table), sizeof(*table));
	memset(table, 0, 256 * sizeof(*table));

	SetTrap(0x00);
	SetTrap(0x01);
	SetTrap(0x02);
	SetTrap(0x03);
	SetTrap(0x04);
	SetTrap(0x05);
	SetTrap(0x06);
	SetTrap(0x07);
	SetTrap(0x08);
	SetTrap(0x09);
	SetTrap(0x0a);
	SetTrap(0x0b);
	SetTrap(0x0c);
	SetTrap(0x0d);
	SetTrap(0x0e);
	SetTrap(0x0f);
	SetTrap(0x10);
	SetTrap(0x11);
	SetTrap(0x12);
	SetTrap(0x13);

	pd.base = (u32)table;
	pd.limit = 256 * sizeof(*table) - 1;
	lidt(&pd);
}

IDT::TrapHandler
IDT::RegisterHandler(u32 idx, TrapHandler h, void *arg)
{
	if (idx >= 256) {
		return 0;
	}
	TrapHandler ret = handlersTable[idx];
	handlersTable[idx] = h;
	hdlArgs[idx] = arg;
	return ret;
}

IDT::TrapHandler
IDT::RegisterUTHandler(TrapHandler h, void *arg)
{
	TrapHandler ret = _unhandledTrap;
	_unhandledTrap = h;
	_unhandledTrapArg = arg;
	return ret;
}

const char *
IDT::StrTrap(SysTraps trap)
{
	const char *str[] = {
		"Divide error",
		"Debug trap",
		"NMI interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND range exceeded",
		"Invalid opcode",
		"Device not available",
		"Double fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment not present",
		"Stack segment fault",
		"General protection",
		"Page fault",
		"Intel reserved",
		"x87 FPU floating point error",
		"Alignment check",
		"Machine check",
		"SIMD floating point exception"
	};

	if ((u32)trap > sizeof(str) / sizeof(str[0])) {
		return "Unknown exception";
	}
	return str[trap];
}