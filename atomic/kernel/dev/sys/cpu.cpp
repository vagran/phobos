/*
 * /kernel/dev/sys/cpu.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#include <sys.h>
phbSource("$Id$");

/*
 * Our per-CPU private data stored in specially created segments (CPU::PrivSegment).
 * The segments are referenced by %fs register of each CPU and contain at least
 * pointer to CPU class instance associated with the given CPU.
 */

DefineDevFactory(CPU);

RegDevClass(CPU, "cpu", Device::T_SPECIAL, "Central Processing Unit");

u32 CPU::numCpus = 0;

SpinLock CPU::startupLock;

ListHead CPU::allCpus;

int CPU::dthRegistered = 0;

CPU::CPU(Type type, u32 unit, u32 classID) : Device(type, unit, classID)
{
	smpGDT = 0;
	initialStack = 0;
	intrNesting = 0;
	intrServiced = 0;
	trapNesting = 0;
	trapsHandled = 0;
	tss = 0;
	setAST = 0;
	memset(&pcpu, 0, sizeof(pcpu));
	LIST_ADD(list, this, allCpus);

	if (numCpus) { /* for APs */
		lidt(idt->GetPseudoDescriptor());
		lgdt(gdt->GetPseudoDescriptor());
		lldt(gdt->GetSelector(GDT::SI_LDT));
	}

	if (unit != numCpus) {
		panic("Attempted to create CPU device with invalid unit index (%u/%u)",
			unit, numCpus);
	}

	GetInfo();
	if (feat1 & CPUID_APIC) {
		lapic = (LAPIC *)devMan.CreateDevice("lapic", unit);
		if (!lapic) {
			klog(KLOG_ERROR, "Unable to create LAPIC device for CPU %u", unit);
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
	SDT::SetDescriptor(privSeg, /*FIXME*/(u64)&privSegData, sizeof(PrivSegment) - 1, 0,
		0, SDT::DST_DATA | SDT::DST_D_WRITE, 0);
	/* private segment is referenced by %fs register of each CPU */
	ASM (
		"mov	%0, %%fs"
		:
		: "r"(privSegSel));

	if (!dthRegistered) {
		idt->RegisterHandler(LAPIC::IPI_VECTOR + LAPIC::IPI_DEACTIVATE_THREAD,
			this, (IDT::TrapHandler)&CPU::DeactivateThreadHandler);
		dthRegistered = 1;
	}

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

void
CPU::NestTrap(int nestIn)
{
	if (nestIn) {
		trapNesting++;
		trapsHandled++;
	} else {
		assert(trapNesting);
		trapNesting--;
	}
}

int
CPU::SendIPI(LAPIC::IPIType type)
{
	if (type >= LAPIC::IPI_MAX) {
		return -1;
	}
	CPU *currentCpu = GetCurrent();
	LAPIC *curLapic = currentCpu->GetLapic();
	return curLapic->SendIPI(LAPIC::DM_FIXED, LAPIC::DST_SPECIFIC, 0,
		LAPIC::IPI_VECTOR + type, GetID());
}

int
CPU::DeactivateThread()
{
	/*
	 * Thread state should be changed to actual value. We send IPI to required
	 * CPU and it will deactivate the thread in PM::ValidateState method.
	 */
	return SendIPI(LAPIC::IPI_DEACTIVATE_THREAD);
}

int
CPU::DeactivateThreadHandler(Frame *frame)
{
	/* call OnUserRet before return from trap */
	SetAST();
	return 0;
}

/* Restore private segment selector from private TSS */
CPU *
CPU::RestoreSelector()
{
	TSS *tss = TSS::GetCurrent();
	if (!tss) {
		return 0;
	}
	u32 size;
	PrivTSS *privTSS = (PrivTSS *)tss->GetPrivateData(&size);
	if (!privTSS || size != sizeof(PrivTSS)) {
		return 0;
	}
	ASM (
		"mov	%0, %%fs"
		:
		: "r"(privTSS->cpu->privSegSel)
	);
	return privTSS->cpu;
}

int
CPU::Activate(StartupFunc func, void *arg)
{
	/*
	 * Create kernel stack. It is used only during early CPU initialization.
	 * When the first threads will be created new stacks will be allocated and
	 * mapped to KSTACK_ADDRESS.
	 */
	initialStack = (u8 *)MM::malloc(KSTACK_SIZE);
	assert(initialStack);

	/* Setup private TSS for this CPU */
	tss = NEWSINGLE(TSS, (void *)(KSTACK_ADDRESS + KSTACK_SIZE), sizeof(PrivTSS));
	ensure(tss);
	PrivTSS *privTSS = (PrivTSS *)tss->GetPrivateData();
	privTSS->cpu = this;
	tss->SetActive();

	/* kernel initial stack for system calls */
	wrmsr(MSR_SYSENTER_ESP_MSR, (u64)KSTACK_ADDRESS + KSTACK_SIZE);

	mm->AttachCPU();

	/* Switch stack and jump to startup code */
	int rc;
	ASM (
		"mov	%1, %%rsp\n"
		"push	%3\n"
		"call	*%2\n"
		: "=a"(rc)
		: "r"(initialStack + KSTACK_SIZE), "r"(func), "r"(arg)
	);
	return rc;
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
	/* use private CPU segment referenced by %fs selector */
	ASM (
		"xor	%0, %0\n"
		"mov	%%fs, %0\n"
		"test	%0, %0\n"
		"jz		1f\n"
		"mov	%%fs:%1, %0\n"
		"1:\n"
		: "=&r"(cpu)
		: "m"(*(u32 *)(void *)OFFSETOF(PrivSegment, cpu))
		: "cc"
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
	MM::Page *pg = mm->GetPage((paddr_t)smpGDT);
	assert(pg);
	pg->Unwire();
	mm->FreePage(pg);
	vsize_t codeSize = (vaddr_t)&APBootEntryEnd - (vaddr_t)&APBootEntry;
	u32 pgNum = (codeSize + PAGE_SIZE - 1) / PAGE_SIZE;
	for (u32 pgIdx = 0; pgIdx < pgNum; pgIdx++) {
		paddr_t pa = mm->Kextract(va);
		mm->UnmapPhys(va);
		MM::Page *pg = mm->GetPage(pa);
		assert(pg);
		pg->Unwire();
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
	pg->Wire();
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
	vsize_t codeSize = (vaddr_t)&APBootEntryEnd - (vaddr_t)&APBootEntry;
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
		pg->Wire();
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

int
APStartup(vaddr_t entryAddr)
{
	/* open gate for a next CPU */
	u32 *lock = (u32 *)(((vaddr_t)&APLock - (vaddr_t)&APBootEntry) + entryAddr);
	AtomicOp::And(lock, 0);
	/* involve the CPU to the processes management */
	pm->AttachCPU();
	NotReached();
}

/* every AP enters here after protected mode and paging is enabled */
ASMCALL void
APBootstrap(vaddr_t entryAddr)
{
	CPU *cpu = CPU::Startup();
	if (!cpu) {
		panic("Failed to create CPU device object");
	}
	cpu->Activate((CPU::StartupFunc)APStartup, (void *)entryAddr);
}
