/*
 * /kernel/dev/sys/cpu.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef CPU_H_
#define CPU_H_
#include <sys.h>
phbSource("$Id$");

class CPU;

#include <dev/sys/lapic.h>

class CPU : public Device {
private:
	static u32 numCpus;
	static SpinLock startupLock;

	LAPIC *lapic;
	char vendorID[13];
	u32 maxCpuid;
	u32 version, ebx1;
	u32 feat1, feat2;

	int GetInfo();
public:
	DeclareDevFactory();
	CPU(Type type, u32 unit, u32 classID);

	static CPU *Startup();
};

#endif /* CPU_H_ */
