/*
 * /kernel/dev/sys/cpu.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

/*
 * Our per-CPU private data stored in specially created segments (CPU::PrivSegment).
 * The segments are referenced by %fs register if each CPU and contain at least
 * pointer to CPU class instance associated with the given CPU.
 */

DefineDevFactory(CPU);

RegDevClass(CPU, "cpu", Device::T_SPECIAL, "Central Processing Unit");

u32 CPU::numCpus = 0;

SpinLock CPU::startupLock;

ListHead CPU::allCpus;

CPU::CPU(Type type, u32 unit, u32 classID) : Device(type, unit, classID)
{
	smpGDT = 0;
	initialStack = 0;
	intrNesting = 0;
	intrServiced = 0;
	LIST_ADD(list, this, allCpus);

	if (numCpus) { /* for APs */
		lidt(idt->GetPseudoDescriptor());
		lgdt(gdt->GetPseudoDescriptor());
		lldt(gdt->GetSelector(GDT::SI_LDT));
	}

	if (unit != numCpus) {
		panic("Attempted to create CPU device with invalid unit index (%lu/%lu)",
			unit, numCpus);
	}

	GetInfo();
	if (feat1 & CPUID_APIC) {
		lapic = (LAPIC *)devMan.CreateDevice("lapic", unit);
		if (!lapic) {
			klog(KLOG_ERROR, "Unable to create LAPIC device for CPU %lu", unit);
		} else {
			lapic->SetCpu(this);
		}
	} else {
		lapic = 0;
	}

	privSeg = gdt->AllocateSegment();
	if (!privSeg) {
		panic("CPU private segment allocation failed");
	}
	privSegSel = gdt->GetSelector(privSeg);
	privSegData.cpu = this;
	SDT::SetDescriptor(privSeg, (u32)&privSegData, sizeof(PrivSegment) - 1, 0,
		0, SDT::DST_DATA | SDT::DST_D_WRITE, 0);
	/* private segment is referenced by %fs register of each CPU */
	__asm__ __volatile__ (
		"movw	%0, %%fs"
		:
		: "r"(privSegSel));
	devState = S_UP;
}

void
CPU::NestInterrupt(int nestIn)
{
	if (nestIn) {
		intrNesting++;
		intrServiced++;
	} else {
		assert(intrNesting);
		intrNesting--;
	}
}

int
CPU::CreateInitialStack(u32 size)
{
	/*
	 * Create initial temporal stack. Calling function should not
	 * return and access its arguments after the stack is switched.
	 */
	initialStack = (u8 *)MM::malloc(size);
	assert(initialStack);
	/* we count stack up to the frame base of the calling function */
	u32 *oldStack = (u32 *)__builtin_frame_address(0);
	u32 delta = (u32)initialStack + size - *oldStack;
	/* Copy old stack content, shift frame base pointer and switch stack pointer. */
	__asm__ __volatile__ (
		"movl	%%esi, %%ecx\n"
		"subl	%%esp, %%ecx\n"
		"incl	%%ecx\n"
		"std\n"
		"rep movsb\n"
		"addl	%2, %%ebp\n"
		"addl	%2, (%%ebp)\n"
		"addl	%2, %%esp\n"
		:
		: "S"(*oldStack - 1), "D"(initialStack + INITIAL_STACK_SIZE - 1),
		  "r"(delta)
		: "ecx", "cc"
	);
	return 0;
}

u32
CPU::GetID()
{
	if (!lapic) {
		return 0;
	}
	return lapic->GetID();
}

LAPIC *
CPU::GetLapic()
{
	return lapic;
}

CPU *
CPU::GetCurrent()
{
	CPU *cpu;
	__asm__ __volatile__ (
		"xorl	%0, %0\n"
		"movl	%%fs, %0\n"
		"testl	%0, %0\n"
		"jz		1f\n"
		"movl	%%fs:%1, %0\n"
		"1:\n"
		: "=&r"(cpu)
		: "m"(*(u32 *)OFFSETOF(PrivSegment, cpu))
	);
	return cpu;
}

int
CPU::GetInfo()
{
	cpuid(0, &maxCpuid, (u32 *)vendorID, (u32 *)&vendorID[8],
		(u32 *)&vendorID[4]);
	vendorID[12] = 0;
	klog(KLOG_INFO, "CPU found, vendor ID: '%s'", vendorID);
	cpuid(1, &version, &ebx1, &feat2, &feat1);
	return 0;
}

CPU *
CPU::Startup()
{
	startupLock.Lock();
	CPU *dev = (CPU *)devMan.CreateDevice("cpu", numCpus);
	startupLock.Unlock();
	if (dev) {
		AtomicOp::Inc(&numCpus);
	}
	return dev;
}

void
CPU::Delay(u32 usec)
{
	/* XXX */
	u64 tsc = rdtsc();
	u64 end = tsc + usec * 1000;
	while ((i64)(end - tsc) > 0) {
		tsc = rdtsc();
		pause();
	}
}

int
CPU::StartAPs(u32 vector)
{
	assert(!(vector & ~0xff));
	u32 x = IM::DisableIntr();
	if (!lapic) {
		return -1;
	}
	lapic->SendIPI(LAPIC::DM_INIT, LAPIC::DST_OTHERS);
	lapic->WaitIPI();
	lapic->SendIPI(LAPIC::DM_INIT, LAPIC::DST_OTHERS, 0, 0, 0,
		LAPIC::DSTMODE_PHYSICAL, LAPIC::LVL_DEASSERT);
	Delay(10000);
	lapic->WaitIPI();
	lapic->SendIPI(LAPIC::DM_STARTUP, LAPIC::DST_OTHERS, 0, vector);
	Delay(200);
	lapic->WaitIPI();
	/*lapic->SendIPI(LAPIC::DM_STARTUP, LAPIC::DST_OTHERS, 0, vector);
	Delay(200);
	lapic->WaitIPI();*/
	IM::RestoreIntr(x);
	return 0;
}

int
CPU::StartSMP()
{
	CPU *cpu = GetCurrent();
	vaddr_t tramp = cpu->InstallTrampoline();
	cpu->StartAPs(tramp >> PAGE_SHIFT);
	return 0;
}

void
CPU::UninstallTrampoline(vaddr_t va)
{
	mm->UnmapPhys(smpGDT);
	/* FreePage() */
	vsize_t codeSize = (u32)&APBootEntryEnd - (u32)&APBootEntry;
	u32 pgNum = (codeSize + PAGE_SIZE - 1) / PAGE_SIZE;
	for (u32 pgIdx = 0; pgIdx < pgNum; pgIdx++) {
		paddr_t pa = mm->Kextract(va);
		mm->UnmapPhys(va);
		MM::Page *pg = mm->GetPage(pa);
		mm->FreePage(pg);
		va += PAGE_SIZE;
	}
	if (initialStack) {
		MM::mfree((void *)initialStack);
		initialStack = 0;
	}
}

vaddr_t
CPU::InstallTrampoline()
{
	/* create GDT copy on identity mapped page */
	void *gdtData = (void *)gdt->GetPseudoDescriptor()->base;
	MM::Page *pg = mm->AllocatePage(0, MM::ZONE_1MB);
	if (!pg) {
		panic("Failed to allocate page for SMP trampoline GDT");
	}
	smpGDT = (vaddr_t)pg->pa;
	mm->MapPhys(smpGDT, pg->pa);
	u32 gdtSize = sizeof(SDT::Descriptor) * 16;
	memcpy((void *)smpGDT, gdtData, gdtSize);

	/* Initialize booting parameters */
	APcodeSel = gdt->GetSelector(GDT::SI_KCODE);
	APdataSel = gdt->GetSelector(GDT::SI_KDATA);
	APPDPT = rcr3();
	APGDTdesc.base = smpGDT;
	APGDTdesc.limit = gdtSize - 1;

	/* map and copy code to real mode memory */
	vsize_t codeSize = (u32)&APBootEntryEnd - (u32)&APBootEntry;
	u32 pgNum = (codeSize + PAGE_SIZE - 1) / PAGE_SIZE;
	/*
	 * Since pages are not continuously allocated paging must be
	 * enabled in the first 4kb of code. Only the first page is allocated
	 * in the first megabyte and is identity mapped.
	 */
	vaddr_t sva = 0;
	for (u32 pgIdx = 0; pgIdx < pgNum; pgIdx++) {
		pg = mm->AllocatePage(0, pgIdx ? MM::ZONE_REST : MM::ZONE_1MB);
		if (!pg) {
			panic("Failed to allocate page for SMP trampoline");
		}
		paddr_t pa = pg->pa;
		if (!pgIdx) {
			assert(pa < 0x100000);
			sva = (vaddr_t)pa;
		}
		vaddr_t va = sva + pgIdx * PAGE_SIZE;
		mm->MapPhys(va, pa);
	}
	APentryAddr = sva;
	memcpy((void *)sva, &APBootEntry, codeSize);
	return sva;
}

/* every AP enters here after protected mode and paging is enabled */
ASMCALL void
APBootstrap(vaddr_t entryAddr)
{
	CPU *cpu = CPU::Startup();
	if (!cpu) {
		panic("Failed to create CPU device object");
	}
	volatile vaddr_t _entryAddr = entryAddr;
	cpu->CreateInitialStack();
	/* After stack is switched we cannot return from this function and access arguments */
	u32 *lock = (u32 *)(((u32)&APLock - (u32)&APBootEntry) + _entryAddr);
	AtomicOp::And(lock, 0);
	if (CPU::GetCpuCount() == 4) {
		//RunDebugger("4 cpus started");//temp
	}
	sti();
	while (1) {
		hlt();
		printf("CPU waken: %lu\n", cpu->GetUnit());
	}
}
