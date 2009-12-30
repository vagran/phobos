/*
 * /kernel/dev/sys/lapic.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

DefineDevFactory(LAPIC);

RegDevClass(LAPIC, "lapic", Device::T_SPECIAL, "Local Advanced Programmable Interrupts Controller");

LAPIC::LAPIC(Type type, u32 unit, u32 classID) : Device(type, unit, classID)
{
	cpu = 0;
	memPhys = mm->AllocDevPhys(MEM_SIZE);
	if (!memPhys) {
		klog(KLOG_ERROR, "No physical memory space for LAPIC device available");
		return;
	}
	memMap = (u8 *)mm->MapPhys(memPhys, MEM_SIZE);
	if (!memMap) {
		mm->FreeDevPhys(memPhys);
		memPhys = 0;
		klog(KLOG_ERROR, "Cannot map LAPIC device memory");
		return;
	}
	/* set base address for memory-mapped I/O */
	u64 x = rdmsr(MSR_APICBASE);
	x = (x & ~APICBASE_ADDRESS) | APICBASE_ENABLED | memPhys;
	wrmsr(MSR_APICBASE, x);

	/* configure local interrupts */
	WriteReg(REG_LVTTMR, LVTEB_MASK); /* disable timer until required */
	WriteReg(REG_LVTTSR, LVTEB_MASK); /* disable thermal sensor until required */
	WriteReg(REG_LVTPMC, LVTEB_MASK); /* disable performance monitoring counter until required */
	WriteReg(REG_LVTER, LVTEB_MASK); /* disable error interrupts until required */
	WriteReg(REG_LINT0, LVTEB_TM | LVTEB_DM_EXTINT); /* accept interrupts from PIC */
	WriteReg(REG_LINT1, LVTEB_DM_NMI); /* accept NMI from external sources */


	devState = S_UP;
}

void
LAPIC::WriteReg(u32 idx, u32 value)
{
	*(volatile u32 *)&memMap[idx] = value;
}

u32
LAPIC::ReadReg(u32 idx)
{
	return *(volatile u32 *)&memMap[idx];
}

int
LAPIC::SetCpu(CPU *cpu)
{
	this->cpu = cpu;
	assert(devUnit == cpu->GetUnit());
	return 0;
}
