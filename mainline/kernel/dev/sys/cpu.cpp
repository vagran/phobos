/*
 * /kernel/dev/sys/cpu.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

DefineDevFactory(CPU);

RegDevClass(CPU, "cpu", Device::T_SPECIAL, "Central Processing Unit");

u32 CPU::numCpus = 0;

SpinLock CPU::startupLock;

CPU::CPU(Type type, u32 unit, u32 classID) : Device(type, unit, classID)
{
	smpGDT = 0;

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
	privSegData = (PrivSegment *)MM::malloc(sizeof(PrivSegment));
	if (!privSegData) {
		panic("CPU private segment space allocation failed");
	}
	privSegData->cpu = this;
	SDT::SetDescriptor(privSeg, (u32)privSegData, sizeof(PrivSegment) - 1, 0,
		0, SDT::DST_DATA | SDT::DST_D_WRITE, 0);
	/* private segment is referenced by %fs register of each CPU */
	__asm__ __volatile__ (
		"movw	%0, %%fs"
		:
		: "m"(privSegSel));
	devState = S_UP;
}

CPU *
CPU::GetCurrent()
{
	CPU *cpu;
	__asm__ __volatile__ (
		"movl	%%fs:0, %0"
		: "=r"(cpu)
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
	u32 x = im->DisableIntr();
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
	im->RestoreIntr(x);
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
		//paddr_t pa = mm->Kextract(va);
		mm->UnmapPhys(va);
		//MM::Page *pg = mm->GetPage(pa);
		/* FreePage() */
		va += PAGE_SIZE;
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
	 * enabled in the first 4kb of code.
	 */
	vaddr_t sva;
	for (u32 pgIdx = 0; pgIdx < pgNum; pgIdx++) {
		pg = mm->AllocatePage(0, MM::ZONE_1MB);
		if (!pg) {
			panic("Failed to allocate page for SMP trampoline");
		}
		paddr_t pa = pg->pa;
		assert(pa < 0x100000);
		if (!pgIdx) {
			sva = (vaddr_t)pa;
		}
		vaddr_t va = sva + pgIdx * PAGE_SIZE;
		mm->MapPhys(va, pa); /* only the first page is identity-mapped */
	}
	APentryAddr = sva;
	memcpy((void *)sva, &APBootEntry, codeSize);
	return sva;
}

ASMCALL void
APBootstrap()
{
	tracec('B');
	AtomicOp::And(&APLock, 0);
	hlt();
}
