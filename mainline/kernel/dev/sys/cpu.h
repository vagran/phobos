/*
 * /kernel/dev/sys/cpu.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
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

	typedef struct {
		CPU *cpu;
	} PrivSegment;

	SDT::Descriptor *privSeg;
	u16 privSegSel;
	PrivSegment *privSegData;
	vaddr_t smpGDT;

	int GetInfo();
	int StartAPs(u32 vector);
	vaddr_t InstallTrampoline();
	void UninstallTrampoline(vaddr_t va);
public:
	DeclareDevFactory();
	CPU(Type type, u32 unit, u32 classID);

	static CPU *Startup(); /* must be called once by every initialized CPU */
	static void Delay(u32 usec);
	static CPU *GetCurrent();
	static int StartSMP();
};

extern "C" u8 APBootEntry, APBootEntryEnd;
extern "C" SDT::PseudoDescriptor APGDTdesc;
extern "C" u16 APcodeSel, APdataSel;
extern "C" u32 APPDPT, APentryAddr, APLock;

#endif /* CPU_H_ */
