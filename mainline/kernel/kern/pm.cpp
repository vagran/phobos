/*
 * /kernel/kern/pm.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

PM *pm;

PM::PM()
{
	LIST_INIT(processes);
	TREE_INIT(pids);
	numProcesses = 0;
	pidFirstSet = -1;
	pidLastUsed = -1;
	memset(pidMap, 0, sizeof(pidMap));
}

PM::pid_t
PM::AllocatePID()
{
	pidMapLock.Lock();
	pid_t pid = AllocateBit(pidMap, MAX_PROCESSES, &pidFirstSet, &pidLastUsed);
	pidMapLock.Unlock();
	return pid;
}

int
PM::ReleasePID(pid_t pid)
{
	pidMapLock.Lock();
	ReleaseBit(pid, pidMap, MAX_PROCESSES, &pidFirstSet, &pidLastUsed);
	pidMapLock.Unlock();
	return 0;
}

PM::Process *
PM::CreateProcess(Thread::ThreadEntry entry, void *arg)
{
	int pidIdx = AllocatePID();
	if (pidIdx == -1) {
		klog(KLOG_WARNING, "No PIDs available for new process");
		return 0;
	}
	pid_t pid = pidIdx;
	Process *proc = NEW(Process);
	if (!proc) {
		ReleasePID(pid);
		return 0;
	}
	if (proc->Initialize()) {
		DELETE(proc);
		ReleasePID(pid);
		klog(KLOG_WARNING, "Failed to initialize new process");
		return 0;
	}
	procListLock.Lock();
	LIST_ADD(list, proc, processes);
	TREE_ADD(pidTree, proc, pids, pid);
	numProcesses++;
	procListLock.Unlock();
	if (!proc->CreateThread(entry, arg)) {
		DestroyProcess(proc);
		return 0;
	}
	return proc;
}

int
PM::DestroyProcess(Process *proc)
{
	assert(proc);
	procListLock.Lock();
	LIST_DELETE(list, proc, processes);
	numProcesses--;
	procListLock.Unlock();
	ReleasePID(TREE_KEY(pidTree, proc));
	DELETE(proc);
	return 0;
}

int
PM::AllocateBit(u8 *bits, int numBits, int *firstSet, int *lastUsed)
{
	int newIdx = (*lastUsed) + 1;
	if (newIdx >= numBits) {
		newIdx = 0;
	}
	if (*firstSet == -1) {
		*firstSet = newIdx;
		*lastUsed = newIdx;
		bitset(bits, newIdx);
		return newIdx;
	}
	if (newIdx != *firstSet) {
		*lastUsed = newIdx;
		bitset(bits, newIdx);
		return newIdx;
	}
	int nextIdx = newIdx + 1;
	do {
		if (nextIdx >= numBits) {
			nextIdx = 0;
		}
		if (bitisclear(bits, nextIdx)) {
			newIdx = nextIdx;
			*lastUsed = newIdx;
			bitset(bits, newIdx);
			nextIdx++;
			if (nextIdx >= numBits) {
				nextIdx = 0;
			}
			/* advance firstSet pointer */
			while (bitisclear(bits, nextIdx)) {
				nextIdx++;
				if (nextIdx >= numBits) {
					nextIdx = 0;
				}
			}
			*firstSet = nextIdx;
			return newIdx;
		}
	} while (nextIdx != newIdx);
	/* nothing found */
	return -1;
}

int
PM::ReleaseBit(int bitIdx, u8 *bits, int numBits, int *firstSet, int *lastUsed)
{
	assert(bitisset(bits, bitIdx));
	bitclear(bits, bitIdx);
	if (bitIdx == *lastUsed) {
		(*lastUsed)--;
		if (*lastUsed < 0) {
			*lastUsed = numBits - 1;
		}
	}
	if (bitIdx == *firstSet) {
		/* find next set */
		int nextIdx = bitIdx + 1;
		do {
			if (nextIdx >= numBits) {
				nextIdx = 0;
			}
			if (bitisset(bits, nextIdx)) {
				*firstSet = nextIdx;
				return 0;
			}
		} while (nextIdx != bitIdx);
		/* no set bits found */
		*firstSet = -1;
	}
	return 0;
}

/***********************************************/
/* Process class */

PM::Process::Process()
{
	LIST_INIT(threads);
	numThreads = 0;
	map = 0;
	userMap = 0;
	gateMap = 0;
}

PM::Process::~Process()
{
	if (map) {
		DELETE(map);
	}
}

PM::Thread *
PM::Process::CreateThread(Thread::ThreadEntry entry, void *arg, u32 stackSize)
{
	Thread *thrd = NEW(Thread, this);
	if (!thrd) {
		return 0;
	}
	if (thrd->Initialize(entry, arg, stackSize)) {
		DELETE(thrd);
		return 0;
	}
	thrdListLock.Lock();
	LIST_ADD(list, thrd, threads);
	numThreads++;
	thrdListLock.Unlock();
	return thrd;
}

int
PM::Process::Initialize()
{
	map = mm->CreateMap();
	if (!map) {
		return -1;
	}
	userMap = map->CreateSubmap(0, GATE_AREA_ADDRESS);
	if (!userMap) {
		return -1;
	}
	gateMap = map->CreateSubmap(GATE_AREA_ADDRESS, GATE_AREA_SIZE);
	if (!gateMap) {
		return -1;
	}
	return 0;
}

/***********************************************/
/* Thread class */

PM::Thread::Thread(Process *proc)
{
	this->proc = proc;
	stackSize = 0;
	stackObj = 0;
	stackEntry = 0;
	cpu = 0;
	memset(&ctx, 0, sizeof(ctx));
}

extern "C" void
OnThreadExit(u32 exitCode, PM::Thread *thrd);

void
OnThreadExit(u32 exitCode, PM::Thread *thrd)
{
	thrd->Exit(exitCode);
}

__asm__ __volatile__ (
	".globl _OnThreadExit\n"
	"_OnThreadExit:\n"
	"pushl	%eax\n"
	"call	OnThreadExit\n");

extern "C" void _OnThreadExit();

int
PM::Thread::Exit(u32 exitCode)
{

	return 0;
}

int
PM::Thread::Initialize(ThreadEntry entry, void *arg, u32 stackSize)
{
	this->stackSize = stackSize;
	stackObj = NEW(MM::VMObject, stackSize, MM::VMObject::F_STACK);
	if (!stackObj) {
		return -1;
	}
	stackEntry = proc->userMap->InsertObject(stackObj, 0, stackObj->GetSize());
	if (!stackEntry) {
		return -1;
	}
	ctx.cs = GDT::GetSelector(GDT::SI_KCODE);
	ctx.ss = ctx.ds = ctx.es = GDT::GetSelector(GDT::SI_KDATA);
	ctx.eip = (u32)entry;
	u32 *esp = (u32 *)(stackEntry->base + stackEntry->size - sizeof(u32));
	*(esp--) = (u32)this; /* argument for exit function */
	*(esp) = (u32)_OnThreadExit;
	ctx.esp = ctx.ebp = (u32)esp;
	return 0;
}

PM::Thread::~Thread()
{
	if (stackObj) {
		stackObj->Release();
	}
}
