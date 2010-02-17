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
	LIST_INIT(runQueues);
	TREE_INIT(tqSleep);
	TREE_INIT(pids);
	numProcesses = 0;
	pidFirstSet = -1;
	pidLastUsed = -1;
	kernelProc = 0;
	kernInitThread = 0;
	idleThread = 0;
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

int
PM::IdleThread(void *arg)
{
	((PM *)arg)->IdleThread();
	return 0;
}

void
PM::IdleThread()
{
	while (1) {
		hlt();
	}
}

/* must be called with tqLock */
PM::SleepEntry *
PM::GetSleepEntry(waitid_t id)
{
	return TREE_FIND(id, SleepEntry, tree, tqSleep);
}

/* must be called with tqLock */
PM::SleepEntry *
PM::CreateSleepEntry(waitid_t id)
{
	SleepEntry *p = NEW(SleepEntry);
	TREE_ADD(tree, p, tqSleep, id);
	LIST_INIT(p->threads);
	p->numThreads = 0;
	return p;
}

/* must be called with tqLock */
void
PM::FreeSleepEntry(SleepEntry *p)
{
	assert(!p->numThreads);
	TREE_DELETE(tree, p, tqSleep);
	DELETE(p);
}

PM::Process *
PM::IntCreateProcess(Thread::ThreadEntry entry, void *arg,
		int priority, int isKernelProc)
{
	pid_t pid = AllocatePID();
	if (pid == INVALID_PID) {
		klog(KLOG_WARNING, "No PIDs available for new process");
		return 0;
	}
	Process *proc = NEW(Process);
	if (!proc) {
		ReleasePID(pid);
		return 0;
	}
	if (proc->Initialize(priority, isKernelProc)) {
		DELETE(proc);
		ReleasePID(pid);
		klog(KLOG_WARNING, "Failed to initialize new process");
		return 0;
	}
	procListLock.Lock();
	LIST_ADD(list, proc, processes);
	proc->pid.isThread = 0;
	proc->pid.proc = proc;
	TREE_ADD(tree, &proc->pid, pids, pid);
	numProcesses++;
	procListLock.Unlock();
	if (!isKernelProc) {
		Thread *thrd = proc->CreateThread(entry, arg);
		if (!thrd) {
			DestroyProcess(proc);
			return 0;
		}
		thrd->Run();/* XXX should select CPU */
	} else {
		kernInitThread = proc->CreateThread(entry, arg,
			Thread::DEF_KERNEL_STACK_SIZE, KERNEL_PRIORITY);
		ensure(kernInitThread);
		kernInitThread->Run();
	}
	return proc;
}

PM::Process *
PM::CreateProcess(Thread::ThreadEntry entry, void *arg, int priority)
{
	return IntCreateProcess(entry, arg, priority);
}

int
PM::DestroyProcess(Process *proc)
{
	assert(proc);
	procListLock.Lock();
	LIST_DELETE(list, proc, processes);
	numProcesses--;
	TREE_DELETE(tree, &proc->pid, pids);
	procListLock.Unlock();
	ReleasePID(TREE_KEY(tree, &proc->pid));
	DELETE(proc);
	return 0;
}

void
PM::AttachCPU(Thread::ThreadEntry kernelProcEntry, void *arg)
{
	CPU *cpu = CPU::GetCurrent();
	assert(cpu);
	/* create runqueue on this CPU */
	cpu->pcpu.runQueue = NEWSINGLE(Runqueue);
	ensure(cpu->pcpu.runQueue);
	rqLock.Lock();
	LIST_ADD(list, ((Runqueue *)cpu->pcpu.runQueue), runQueues);
	rqLock.Unlock();
	/* The first caller should create kernel process */
	assert((!kernelProc && kernelProcEntry) || (kernelProc && !kernelProcEntry));
	if (kernelProcEntry) {
		/* create kernel process */
		kernelProc = IntCreateProcess(kernelProcEntry, arg, KERNEL_PRIORITY, 1);
	}
	ensure(kernelProc);
	/* create idle thread */
	idleThread = kernelProc->CreateThread(IdleThread, this,
		Thread::DEF_KERNEL_STACK_SIZE, IDLE_PRIORITY);
	ensure(idleThread);
	idleThread->Run();
	if (kernelProcEntry) {
		kernInitThread->SwitchTo();
	} else {
		idleThread->SwitchTo();
	}
	NotReached();
}

int
PM::Sleep(waitid_t channelID, const char *sleepString)
{
	Thread *thrd = Thread::GetCurrent();
	Runqueue *rq = thrd->GetRunqueue();
	rq->RemoveThread(thrd);
	tqLock.Lock();
	SleepEntry *se = GetSleepEntry(channelID);
	if (!se) {
		se = CreateSleepEntry(channelID);
	}
	assert(se);
	LIST_ADD(sleepList, thrd, se->threads);
	se->numThreads++;
	thrd->Sleep(se, sleepString);
	tqLock.Unlock();
	Thread *next = rq->SelectThread();
	/* at least idle thread should be there */
	assert(next);
	next->SwitchTo();
	return 0;
}

int
PM::Sleep(void *channelID, const char *sleepString)
{
	return Sleep((waitid_t)channelID, sleepString);
}

/* return 0 if someone waken, 1 if nobody waits on this channel, -1 if error */
int
PM::Wakeup(waitid_t channelID)
{
	tqLock.Lock();
	SleepEntry *se = GetSleepEntry(channelID);
	if (!se) {
		tqLock.Unlock();
		return 1;
	}
	Thread *thrd;
	while ((thrd = LIST_FIRST(Thread, sleepList, se->threads))) {
		LIST_DELETE(sleepList, thrd, se->threads);
		assert(se->numThreads);
		se->numThreads--;
		thrd->Unsleep();
		thrd->Run(); /* XXX CPU should be selected */
	}
	FreeSleepEntry(se);
	tqLock.Unlock();
	return 0;
}

int
PM::Wakeup(void *channelID)
{
	return Wakeup((waitid_t)channelID);
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
		BitSet(bits, newIdx);
		return newIdx;
	}
	if (newIdx != *firstSet) {
		*lastUsed = newIdx;
		BitSet(bits, newIdx);
		return newIdx;
	}
	int nextIdx = newIdx + 1;
	do {
		if (nextIdx >= numBits) {
			nextIdx = 0;
		}
		if (BitIsClear(bits, nextIdx)) {
			newIdx = nextIdx;
			*lastUsed = newIdx;
			BitSet(bits, newIdx);
			nextIdx++;
			if (nextIdx >= numBits) {
				nextIdx = 0;
			}
			/* advance firstSet pointer */
			while (BitIsClear(bits, nextIdx)) {
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
	assert(BitIsSet(bits, bitIdx));
	BitClear(bits, bitIdx);
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
			if (BitIsSet(bits, nextIdx)) {
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
/* Runqueue class */

PM::Runqueue::Runqueue()
{
	cpu = CPU::GetCurrent();
	ensure(cpu);
	for (int i = 0; i < NUM_PRIORITIES; i++) {
		LIST_INIT(queues[0].queue[i]);
		LIST_INIT(queues[1].queue[i]);
	}
	qActive = &queues[0];
	qExpired = &queues[1];
	curThread = 0;
	numQueued = 0;
	numActive = 0;
	memset(queuedMask, 0, sizeof(queuedMask));
	memset(activeMask, 0, sizeof(activeMask));
}

u32
PM::Runqueue::GetSliceTicks(Thread *thrd)
{
	return tm->MS(MIN_SLICE + (NUM_PRIORITIES - 1 - thrd->priority) *
		(MAX_SLICE - MIN_SLICE) / NUM_PRIORITIES);
}

int
PM::Runqueue::AddThread(Thread *thrd)
{
	assert(thrd->state != Thread::S_RUNNING);
	assert(thrd->priority < NUM_PRIORITIES);
	thrd->sliceTicks = GetSliceTicks(thrd);
	thrd->rqFlags = 0;
	thrd->state = Thread::S_RUNNING;
	qLock.Lock();
	thrd->rqQueue = qActive;
	thrd->cpu = cpu;
	LIST_ADDLAST(rqList, thrd, qActive->queue[thrd->priority]);
	BitSet(activeMask, thrd->priority);
	BitSet(queuedMask, thrd->priority);
	numQueued++;
	numActive++;
	qLock.Unlock();
	return 0;
}

int
PM::Runqueue::RemoveThread(Thread *thrd)
{
	assert(thrd->cpu->pcpu.runQueue == this);
	qLock.Lock();
	Queue *queue = (Queue *)thrd->rqQueue;
	ListHead *head = &queue->queue[thrd->priority];
	LIST_DELETE(rqList, thrd, *head);
	if (LIST_ISEMPTY(*head)) {
		if (queue == qActive) {
			BitClear(activeMask, thrd->priority);
			if (LIST_ISEMPTY(qExpired->queue[thrd->priority])) {
				BitClear(queuedMask, thrd->priority);
			}
		} else {
			assert(queue == qExpired);
			if (LIST_ISEMPTY(qActive->queue[thrd->priority])) {
				BitClear(queuedMask, thrd->priority);
			}
		}
	}
	if (queue == qActive) {
		assert(numActive);
		numActive--;
	}
	assert(numQueued);
	numQueued--;
	thrd->cpu = 0;
	thrd->state = Thread::S_NONE;
	qLock.Unlock();
	return 0;
}

/* must be called with qLock */
PM::Thread *
PM::Runqueue::SelectThread()
{
	assert(numQueued);
	if (!numActive) {
		/* start next round */
		Queue *x = qActive;
		qActive = qExpired;
		qExpired = x;
		numActive = numQueued;
		memcpy(activeMask, queuedMask, sizeof(activeMask));
	}
	int pri = BitFirstSet(activeMask, sizeof(activeMask) * NBBY);
	assert(pri != -1 && pri < NUM_PRIORITIES);
	assert(!LIST_ISEMPTY(qActive->queue[pri]));
	Thread *thrd = LIST_FIRST(Thread, rqList, qActive->queue[pri]);
	return thrd;
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
PM::Process::CreateThread(Thread::ThreadEntry entry, void *arg, u32 stackSize, u32 priority)
{
	pid_t pid = pm->AllocatePID();
	if (pid == INVALID_PID) {
		klog(KLOG_WARNING, "No PIDs available for new thread");
		return 0;
	}
	Thread *thrd = NEW(Thread, this);
	if (!thrd) {
		pm->ReleasePID(pid);
		return 0;
	}
	thrd->pid.isThread = 1;
	thrd->pid.thrd = thrd;
	pm->procListLock.Lock();
	TREE_ADD(tree, &thrd->pid, pm->pids, pid);
	pm->procListLock.Unlock();
	if (thrd->Initialize(entry, arg, stackSize, priority)) {
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
PM::Process::Initialize(u32 priority, int isKernelProc)
{
	if (isKernelProc) {
		this->priority = KERNEL_PRIORITY;
		map = mm->kmemMap;
		gateMap = 0;
	} else {
		this->priority = priority;
		map = mm->CreateMap();
		if (!map) {
			return -1;
		}
		gateMap = map->CreateSubmap(GATE_AREA_ADDRESS, GATE_AREA_SIZE);
		if (!gateMap) {
			return -1;
		}
	}
	userMap = map->CreateSubmap(0, GATE_AREA_ADDRESS);
	if (!userMap) {
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
	priority = DEF_PRIORITY;
	waitEntry = 0;
	waitString = 0;
	rqQueue = 0;
	state = S_NONE;
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
	printf("Thread %d exited\n", GetID());//temp
	//notimpl
	return 0;
}

/* return 0 for caller, 1 for restored thread */
int
PM::Thread::SaveContext(Context *ctx)
{
	int rc;
	__asm__ __volatile__ (
		/* save general purpose registers in stack */
		"pushl	%%ebx\n"
		"pushl	%%ecx\n"
		"pushl	%%edx\n"
		"pushl	%%esi\n"
		"pushl	%%edi\n"
		/* save context */
		"movl	%%ebp, %0\n"
		"movl	%%esp, %1\n"
		"movl	$1f, %2\n"
		"xorl	%%eax, %%eax\n" /* return 0 for caller */
		"jmp	2f\n"
		"1: movl	$1, %%eax\n" /* ... and 1 for restored thread */
		/* restore general purpose registers */
		"2:\n"
		"popl	%%edi\n"
		"popl	%%esi\n"
		"popl	%%edx\n"
		"popl	%%ecx\n"
		"popl	%%ebx\n"
		: "=m"(ctx->ebp), "=m"(ctx->esp), "=m"(ctx->eip), "=&a"(rc)
	);
	return rc;
}

/* this method actually returns in SaveContext() */
void
PM::Thread::RestoreContext(Context *ctx, u32 asRoot)
{
	__asm__ __volatile__ (
		/* preload values in registers because they can be addressed by %ebp/%esp */
		"movl	%0, %%eax\n"
		"movl	%1, %%ebx\n"
		"movl	%%eax, %%ebp\n"
		"movl	%%ebx, %%esp\n"
		/* switch address space if required */
		"testl	%3, %3\n"
		"jz		1f\n"
		"movl	%3, %%cr3\n"
		"1:\n"

		/* and jump to saved %eip */
		"movl	%2, %%eax\n"
		"pushl	%%eax\n"
		"ret\n"
		:
		: "m"(ctx->ebp), "m"(ctx->esp), "m"(ctx->eip), "r"(asRoot)
		: "eax", "ebx"
	);
}

PM::Thread *
PM::Thread::GetCurrent()
{
	CPU *cpu = CPU::GetCurrent();
	Runqueue *rq = (Runqueue *)cpu->pcpu.runQueue;
	assert(rq);
	return rq->GetCurrentThread();
}

int
PM::Thread::Run(CPU *cpu)
{
	assert(state == S_NONE);
	if (!cpu) {
		cpu = CPU::GetCurrent();
	}
	Runqueue *rq = (Runqueue *)cpu->pcpu.runQueue;
	return rq->AddThread(this);
}

int
PM::Thread::Sleep(void *waitEntry, const char *waitString)
{
	assert(state == S_NONE);
	this->waitEntry = waitEntry;
	this->waitString = waitString;
	state = S_SLEEP;
	return 0;
}

int
PM::Thread::Unsleep()
{
	assert(state == S_SLEEP);
	state = S_NONE;
	waitEntry = 0;
	waitString = 0;
	return 0;
}

/* thread must be in same CPU runqueue with the current thread */
void
PM::Thread::SwitchTo()
{
	assert(state == S_RUNNING);
	assert(cpu == CPU::GetCurrent());
	Thread *prev = GetCurrent();
	/* switch address space if it is another process */
	u32 asRoot;
	if (!prev || prev->proc != proc) {
		asRoot = proc->map->GetCR3();
	} else {
		asRoot = 0;
	}
	if (prev) {
		if (prev->SaveContext(&prev->ctx)) {
			/* switched to this thread */
			return;
		}
	}
	Runqueue *rq = GetRunqueue();
	rq->curThread = this;
	RestoreContext(&ctx, asRoot);
}

int
PM::Thread::Initialize(ThreadEntry entry, void *arg, u32 stackSize, u32 priority)
{
	this->stackSize = stackSize;
	this->priority = priority;
	stackObj = NEW(MM::VMObject, stackSize, MM::VMObject::F_STACK);
	if (!stackObj) {
		return -1;
	}
	stackEntry = proc->userMap->InsertObject(stackObj, 0, stackObj->GetSize());
	if (!stackEntry) {
		return -1;
	}
	ctx.eip = (u32)entry;
	u32 *esp = (u32 *)(stackEntry->base + stackEntry->size);
	/*
	 * Prefault required amount of kernel stack space.
	 */
	ensure(!MapKernelStack((vaddr_t)esp));
	/*
	 * Create initial content in stack. Since it is mapped only in
	 * another address space we need to use temporal mappings
	 * for the stack physical pages.
	 */
	esp--;
	paddr_t pa = proc->userMap->Extract((vaddr_t)esp);
	assert(pa);
	u32 *_esp = (u32 *)mm->QuickMapEnter(pa);
	*(_esp--) = (u32)this; /* argument for exit function */
	esp--;
	*(_esp--) = (u32)arg; /* argument for entry point */
	esp--;
	*(_esp) = (u32)_OnThreadExit; /* return address */
	mm->QuickMapRemove((vaddr_t)_esp);
	ctx.esp = ctx.ebp = (u32)esp;
	return 0;
}

/* assure the kernel will have required amount of stack space */
int
PM::Thread::MapKernelStack(vaddr_t esp)
{
	assert(esp > stackEntry->base && esp <= stackEntry->base + stackEntry->size);
	vaddr_t startVa = rounddown2(esp - CPU::DEF_KERNEL_STACK_SIZE, PAGE_SIZE);
	ensure(startVa >= stackEntry->base);
	vaddr_t endVa = rounddown2(esp, PAGE_SIZE);
	for (vaddr_t va = startVa; va < endVa; va += PAGE_SIZE) {
		if (proc->userMap->IsMapped(va)) {
			/* assume that stack is continuously mapped from the bottom */
			break;
		}
		ensure(!proc->userMap->Pagein(va));
	}
	return 0;
}

PM::Thread::~Thread()
{
	if (stackObj) {
		stackObj->Release();
	}
}
