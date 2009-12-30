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

	devState = S_UP;
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
