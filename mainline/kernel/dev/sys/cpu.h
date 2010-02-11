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
public:
	typedef int (*StartupFunc)(void *arg);

	ListEntry list;
	static ListHead allCpus;
private:
	enum {
		DEF_KERNEL_STACK_SIZE = 128 * 1024,
	};

	static u32 numCpus;
	static SpinLock startupLock;

	LAPIC *lapic;
	char vendorID[13];
	u32 maxCpuid;
	u32 version, ebx1;
	u32 feat1, feat2;
	u32 intrNesting;
	u32 intrServiced;

	typedef struct {
		CPU *cpu;
	} PrivSegment;

	SDT::Descriptor *privSeg;
	u16 privSegSel;
	PrivSegment privSegData;
	vaddr_t smpGDT;
	u8 *initialStack;
	TSS *tss;

	int GetInfo();
	int StartAPs(u32 vector);
	vaddr_t InstallTrampoline();
	void UninstallTrampoline(vaddr_t va);
public:
	DeclareDevFactory();
	CPU(Type type, u32 unit, u32 classID);

	static CPU *Startup(); /* must be called once by every initialized CPU */
	static void Delay(u32 usec);
	static CPU *GetCurrent(); /* return 0 if not yet attached */
	static int StartSMP();
	static inline u32 GetCpuCount() { return numCpus; }

	u32 GetID();
	LAPIC *GetLapic();
	int Activate(StartupFunc func, void *arg = 0, u32 stackSize = DEF_KERNEL_STACK_SIZE);
	void NestInterrupt(int nestIn = 1); /* nestIn zero for nest out */
};

extern "C" u8 APBootEntry, APBootEntryEnd, APstack;
extern "C" SDT::PseudoDescriptor APGDTdesc;
extern "C" u16 APcodeSel, APdataSel;
extern "C" u32 APPDPT, APentryAddr, APLock;

#endif /* CPU_H_ */
