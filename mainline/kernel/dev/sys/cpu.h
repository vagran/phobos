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
public:
	enum {
		DEF_KERNEL_STACK_SIZE = 128 * 1024,
		MAX_TRAP_NESTING = 16,
	};

	typedef int (*StartupFunc)(void *arg);

	ListEntry list;
	static ListHead allCpus;

	/* any per CPU handles could be stored here */
	struct PerCpuData {
		void *runQueue; /* PM::Runqueue * */
		void *idleThread; /* PM::Thread * */
	} pcpu;

private:

	static u32 numCpus;
	static SpinLock startupLock;

	LAPIC *lapic;
	char vendorID[13];
	u32 maxCpuid;
	u32 version, ebx1;
	u32 feat1, feat2;
	u32 intrNesting;
	u32 trapNesting;
	u64 intrServiced, trapsHandled;

	/* private per-CPU segment referenced by %fs selector */
	typedef struct {
		CPU *cpu;
	} PrivSegment;

	/* private data in tail of CPU private TSS segment */
	typedef struct {
		CPU *cpu;
	} PrivTSS;

	SDT::Descriptor *privSeg;
	u16 privSegSel;
	PrivSegment privSegData;
	vaddr_t smpGDT;
	u8 *initialStack;
	TSS *tss;
	static int dthRegistered; /* DeactivateThreadHandler registered */
	int setAST;

	int GetInfo();
	int StartAPs(u32 vector);
	vaddr_t InstallTrampoline();
	void UninstallTrampoline(vaddr_t va);
	int DeactivateThreadHandler(Frame *frame);
public:
	DeclareDevFactory();
	CPU(Type type, u32 unit, u32 classID);

	static CPU *Startup(); /* must be called once by every initialized CPU */
	static void Delay(u32 usec);
	static CPU *GetCurrent(); /* return 0 if not yet attached */
	static int StartSMP();
	static inline u32 GetCpuCount() { return numCpus; }
	static CPU *RestoreSelector();

	u32 GetID();
	LAPIC *GetLapic();
	int Activate(StartupFunc func, void *arg = 0, u32 stackSize = DEF_KERNEL_STACK_SIZE);
	void NestInterrupt(int nestIn = 1); /* nestIn zero for nest out */
	void NestTrap(int nestIn = 1); /* nestIn zero for nest out */
	int SendIPI(LAPIC::IPIType type);
	int DeactivateThread(); /* Deactivate currently active thread */
	/* Asynchronous System Trap, call OnUserRet when returning from trap */
	inline void SetAST() { setAST = 1; }
	inline int IsSetAST() { return setAST; }
	inline void ClearAST() { setAST = 0; }
	inline int GetIntrNesting() { return intrNesting; }
	inline int GetTrapNesting() { return trapNesting; }
};

extern "C" u8 APBootEntry, APBootEntryEnd, APstack;
extern "C" SDT::PseudoDescriptor APGDTdesc;
extern "C" u16 APcodeSel, APdataSel;
extern "C" u32 APPDPT, APentryAddr, APLock;

#endif /* CPU_H_ */
