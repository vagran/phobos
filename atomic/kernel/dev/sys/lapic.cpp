/*
 * /kernel/dev/sys/lapic.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
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
	memMap = (u8 *)mm->MapDevPhys(memPhys, MEM_SIZE);
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

	ID = ReadReg(REG_ID) >> 24;
	klog(KLOG_INFO, "LAPIC attached, ID = %u", ID);
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

int
LAPIC::SendIPI(DeliveryMode dm, Destination dst, u32 wait, u32 vector, u32 ID,
	int destMode, int level, int triggerMode)
{
	if (dst == DST_SPECIFIC) {
		WriteReg(REG_ICR + 0x10, ID << 24);
	}
	WriteReg(REG_ICR, (vector & 0xff) |
		((dm & 0x7) << 8) |
		((dst & 0x3) << 18) |
		((destMode & 0x1) << 11) |
		((level & 0x1) << 14) |
		((triggerMode & 0x1) << 15));
	return wait ? WaitIPI(wait) : 0;
}

int
LAPIC::WaitIPI(u32 delay)
{
	while (1) {
		if (!(ReadReg(REG_ICR) & REG_ICR_DS)) {
			return 0;
		}
		if (!delay) {
			break;
		}
		if (delay != 0xffffffff) {
			delay--;
		}
		pause();
	}
	return -1;
}
