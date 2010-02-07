/*
 * /kernel/kern/pm.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef PM_H_
#define PM_H_
#include <sys.h>
phbSource("$Id$");

/* Processes manager */

class PM {
public:
	enum {
		MAX_PROCESSES = 65536,
	};
	typedef u16 pid_t;

public:
	class Process;

	class Thread {
	public:
		enum {
			DEF_STACK_SIZE =		16 * 1024 * 1024, /* Default stack size */
		};
		typedef int (*ThreadEntry)(void *arg);

		typedef struct {
			u32 eax, ebx, ecx, edx,
				esi, edi,
				esp, ebp, eip;
			u16 cs, ss, ds, es;
			/* XXX FPU state */
		} Context;
	private:
		friend class Process;

		ListEntry list; /* list of threads in process */
		Process *proc;
		u32 stackSize;
		vaddr_t stackAddr;
		MM::VMObject *stackObj;
		MM::Map::Entry *stackEntry;
		CPU *cpu;
		Context ctx;

	public:
		Thread(Process *proc);
		~Thread();

		int Initialize(ThreadEntry entry, void *arg = 0, u32 stackSize = DEF_STACK_SIZE);
		int Exit(u32 exitCode);
	};

	class Process {
	private:
		friend class PM;

		ListEntry list; /* list of all processes in PM */
		Tree<pid_t>::TreeEntry pidTree; /* key is PID */
		ListHead threads;
		u32 numThreads;
		SpinLock thrdListLock;
		MM::Map *map, *userMap, *gateMap;
	public:
		Process();
		~Process();

		int Initialize();

		Thread *CreateThread(Thread::ThreadEntry entry, void *arg = 0,
			u32 stackSize = Thread::DEF_STACK_SIZE);
	};


private:
	ListHead processes;
	u32 numProcesses;
	Tree<pid_t>::TreeRoot pids;
	SpinLock procListLock;
	u8 pidMap[MAX_PROCESSES / NBBY];
	int pidFirstSet, pidLastUsed;
	SpinLock pidMapLock;

	pid_t AllocatePID();
	int ReleasePID(pid_t pid);
public:
	PM();
	Process *CreateProcess(Thread::ThreadEntry entry, void *arg = 0);
	int DestroyProcess(Process *proc);

	/* returns index or -1 if no bits available */
	static int AllocateBit(u8 *bits, int numBits, int *firstSet, int *lastUsed);
	static int ReleaseBit(int bitIdx, u8 *bits, int numBits, int *firstSet, int *lastUsed);
};

extern PM *pm;

#endif /* PM_H_ */
