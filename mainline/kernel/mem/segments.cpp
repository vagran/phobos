/*
 * /kernel/mem/segments.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

GDT *gdt;
IDT *idt;

void
SDT::SetDescriptor(Descriptor *d, u32 base, u32 limit, u32 ring, u32 system,
	u32 type, int is16bit)
{
	if ((limit & 0xfff) != 0xfff || limit < 0x100000) {
		if (limit > 0xfffff) {
			panic("Specifying segment with 1-byte granularity and limit >1MB, "
				"limit = 0x%08lX", limit);
		}
		d->gran = 0;
	} else {
		limit >>= 12;
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
	d->def32 = is16bit ? 0 : 1;
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

u32
SDT::GetBase(Descriptor *d)
{
	return d->lobase | (d->hibase << 24);
}

u32
SDT::GetLimit(Descriptor *d)
{
	u32 limit = d->lolimit | (d->hilimit << 16);
	if (d->gran) {
		limit <<= 12;
	}
	return limit;
}

/*************************************************************/

GDT::GDT()
{
	ldt = (ldtTable *)MM::malloc(sizeof(ldtTable), sizeof(SDT::Descriptor));
	memset(ldt, 0 ,sizeof(*ldt));
	table = (Table *)MM::malloc(sizeof(SDT::Descriptor) * GDT_SIZE, sizeof(SDT::Descriptor));
	memset(table, 0, sizeof(SDT::Descriptor) * GDT_SIZE);
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
	pd.limit = sizeof(SDT::Descriptor) * GDT_SIZE - 1;
	__asm __volatile (
		"lgdt	%0\n"
		"pushl	%1\n"
		"pushl	$0f\n"
		"lret\n"
		"0: mov	%2, %%ss\n"
		"mov	%2, %%ds\n"
		"mov	%2, %%es\n"
		"xorl	%2, %2\n"
		"mov	%2, %%fs\n"
		"mov	%2, %%gs\n"
		"lldt	%3\n"
		:
		:"m"(pd), "r"((u32)GetSelector(SI_KCODE, PL_KERNEL)),
		 "r"((u32)GetSelector(SI_KDATA, PL_KERNEL)),
		 "r"(GetSelector(SI_LDT, PL_KERNEL))
		: "cc"
		);
}

SDT::Descriptor *
GDT::AllocateSegment()
{
	for (u32 idx = SI_CUSTOM; idx < GDT_SIZE; idx++) {
		SDT::Descriptor *d = GetDescriptor(idx);
		if (!d->p) {
			return d;
		}
	}
	return 0;
}

int
GDT::ReleaseSegment(SDT::Descriptor *d)
{
	u32 idx = GetIndex(d);
	if (idx < SI_CUSTOM || idx >= GDT_SIZE) {
		return -1;
	}
	memset(d, 0, sizeof(*d));
	return 0;
}

u32
GDT::GetIndex(SDT::Descriptor *d)
{
	u32 idx = ((u32)d - (u32)&table->null) / sizeof(SDT::Descriptor);
	if (idx >= GDT_SIZE) {
		return 0;
	}
	return idx;
}

u16
GDT::GetSelector(SDT::Descriptor *d, u16 rpl)
{
	u32 idx = GetIndex(d);
	if (!idx) {
		return 0;
	}
	return GetSelector(idx, rpl);
}

/*************************************************************/

IDT::IDT()
{
	memset(handlers, 0, sizeof(handlers));
	memset(&utHandler, 0, sizeof(utHandler));

	table = (SDT::Gate *)MM::malloc(NUM_VECTORS * sizeof(*table), sizeof(*table));
	memset(table, 0, NUM_VECTORS * sizeof(*table));

	trapEntrySize = roundup2((u32)&TrapEntryEnd - (u32)&TrapEntry, 0x10);
	TrapTable = MM::malloc(NUM_VECTORS * trapEntrySize, 0x10);
	/* initialize with NOPs */
	memset(TrapTable, 0x90, NUM_VECTORS * trapEntrySize);

	for (u32 idx = 0; idx < NUM_VECTORS; idx++) {
		SDT::SysSegType type;
		if (idx < ST_NUMRESERVED && idx != ST_NMI) {
			type = SDT::SST_TGT32;
		} else {
			type = SDT::SST_IGT32;
		}
		void *entry = (void *)((u32)TrapTable + idx * trapEntrySize);
		SDT::SetGate(&table[idx], (u32)entry,
			GDT::GetSelector(GDT::SI_KCODE, 0), type);
		memcpy(entry, &TrapEntry, (u32)&TrapEntryEnd - (u32)&TrapEntry);
	}

	pd.base = (u32)table;
	pd.limit = NUM_VECTORS * sizeof(*table) - 1;
	lidt(&pd);
}

int
IDT::HandleTrap(Frame *frame)
{
	assert(frame->vectorIdx < NUM_VECTORS);
	TableEntry *p = &handlers[frame->vectorIdx];
	if (!p->handler) {
		if (!utHandler.handler) {
			u32 esp;
			if (((SDT::SegSelector *)(void *)&frame->cs)->rpl) {
				esp = frame->esp;
			} else {
				esp = (u32)&frame->esp;
			}
			panic("Unhandled trap\nidx = %lx (%s), code = %lu, eip = 0x%08lx, eflags = 0x%08lx\n"
				"eax = 0x%08lx, ebx = 0x%08lx, ecx = 0x%08lx, edx = 0x%08lx\n"
				"esi = 0x%08lx, edi = 0x%08lx, ebp = 0x%08lx, esp = 0x%08lx",
				frame->vectorIdx, IDT::StrTrap((IDT::SysTraps)frame->vectorIdx),
				frame->code, frame->eip, frame->eflags,
				frame->eax, frame->ebx, frame->ecx, frame->edx,
				frame->esi, frame->edi, frame->ebp, esp);
		}
		return (p->obj->*utHandler.handler)(frame);
	}
	return (p->obj->*p->handler)(frame);
}

IDT::TrapHandler
IDT::RegisterHandler(u32 idx, Object *obj, TrapHandler h)
{
	if (idx >= NUM_VECTORS) {
		return 0;
	}
	TrapHandler ret = handlers[idx].handler;
	handlers[idx].handler = h;
	handlers[idx].obj = obj;
	return ret;
}

IDT::TrapHandler
IDT::RegisterUTHandler(Object *obj, TrapHandler h)
{
	TrapHandler ret = utHandler.handler;
	utHandler.handler = h;
	utHandler.obj = obj;
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
		"Co-processor Segment Overrun",
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

	if ((u32)trap >= sizeof(str) / sizeof(str[0])) {
		return "Unknown exception";
	}
	return str[trap];
}

/*************************************************************/

TSS *defTss;

TSS::TSS(void *kernelStack, u32 privateDataSize)
{
	dataSize = OFFSETOF(TssSegment, privateData) + privateDataSize;
	data = (TssSegment *)MM::malloc(dataSize, PAGE_SIZE);
	desc = gdt->AllocateSegment();
	ensure(desc);
	memset(data, 0, dataSize);
	data->tss = this;
	data->data.tss_esp0 = (u32)kernelStack;
	data->data.tss_ss0 = GDT::GetSelector(GDT::SI_KDATA);
	SDT::SetDescriptor(desc, (u32)data, dataSize - 1, 0, 1, SDT::SST_TSS32, 0);
}

int
TSS::SetActive()
{
	ltr(gdt->GetSelector(desc));
	return 0;
}

void *
TSS::GetPrivateData(u32 *size)
{
	if (size) {
		*size = dataSize - OFFSETOF(TssSegment, privateData);
	}
	return data->privateData;
}

TSS *
TSS::GetCurrent()
{
	u16 selector = str();
	if (!selector) {
		return 0;
	}
	SDT::Descriptor *desc = gdt->GetDescriptor(selector);
	TssSegment *seg = (TssSegment *)SDT::GetBase(desc);
	return seg->tss;
}
