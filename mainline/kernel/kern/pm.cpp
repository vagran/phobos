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

ListHead PM::imageLoaders;

PM::PM() : pidMap(MAX_PROCESSES)
{
	LIST_INIT(processes);
	LIST_INIT(runQueues);
	TREE_INIT(tqSleep);
	TREE_INIT(pids);
	numProcesses = 0;
	kernelProc = 0;
	kernInitThread = 0;
}

int
PM::RegisterIL(const char *desc, ILFactory factory, ILProber prober)
{
	ILEntry *ile = NEW(ILEntry);
	ile->desc = desc;
	ile->factory = factory;
	ile->prober = prober;
	LIST_ADD(list, ile, imageLoaders);
	return 0;
}

PM::ImageLoader *
PM::GetImageLoader(VFS::File *file)
{
	ILEntry *ile;
	LIST_FOREACH(ILEntry, list, ile, imageLoaders) {
		if (!ile->prober(file)) {
			return ile->factory(file);
		}
	}
	return 0;
}

PM::pid_t
PM::AllocatePID()
{
	pidMapLock.Lock();
	pid_t pid = pidMap.AllocateBit();
	pidMapLock.Unlock();
	return pid;
}

int
PM::ReleasePID(pid_t pid)
{
	pidMapLock.Lock();
	pidMap.ReleaseBit(pid);
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
	CPU *cpu = CPU::GetCurrent();
	assert(cpu);
	Runqueue *rq = (Runqueue *)cpu->pcpu.runQueue;
	assert(rq);
	while (1) {
		Thread *thrd = rq->SelectThread();
		assert(thrd);
		if (thrd != cpu->pcpu.idleThread) {
			thrd->SwitchTo();
		}
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
	p->flags = 0;
	LIST_INIT(p->threads);
	p->numThreads = 0;
	TREE_ADD(tree, p, tqSleep, id);
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
		int priority, int isKernelProc, int runIt)
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
		if (runIt) {
			thrd->Run();/* XXX should select CPU */
		}
	} else {
		kernInitThread = proc->CreateThread(entry, arg,
			Thread::DEF_KERNEL_STACK_SIZE, KERNEL_PRIORITY);
		ensure(kernInitThread);
		if (runIt) {
			kernInitThread->Run();
		}
	}
	return proc;
}

PM::Process *
PM::CreateProcess(Thread::ThreadEntry entry, void *arg, int priority)
{
	return IntCreateProcess(entry, arg, priority);
}

int
PM::ProcessEntry(void *arg)
{
	Thread *thrd = Thread::GetCurrent();
	Process *proc = thrd->GetProcess();
	(void)proc;//temp
	//notimpl
	return 0;
}

PM::Process *
PM::CreateProcess(const char *path, int priority)
{
	VFS::File *file = vfs->CreateFile(path);
	if (!file) {
		return 0;
	}
	ImageLoader *il = GetImageLoader(file);
	if (!il) {
		return 0;
	}
	Process *proc = IntCreateProcess(ProcessEntry, 0, priority, 0, 0);
	if (!proc) {
		il->Release();
		return 0;
	}
	if (il->Load(proc->userMap)) {
		DestroyProcess(proc);
		il->Release();
		return 0;
	}
	proc->SetEntryPoint(il->GetEntryPoint());
	il->Release();
	Thread *thrd = proc->GetThread();
	thrd->Run();
	return proc;
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
	ReleasePID(proc->GetID());
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
	Thread *idleThread = kernelProc->CreateThread(IdleThread, this,
		Thread::DEF_KERNEL_STACK_SIZE, IDLE_PRIORITY);
	ensure(idleThread);
	idleThread->Run();
	cpu->pcpu.idleThread = idleThread;
	if (kernelProcEntry) {
		kernInitThread->SwitchTo();
	} else {
		idleThread->SwitchTo();
	}
	NotReached();
}

int
PM::SleepTimeout(Handle h, u64 ticks, void *arg)
{
	pm->SleepTimeout((Thread *)arg);
	return 0;
}

void
PM::SleepTimeout(Thread *thrd)
{
	u64 x = im->SetPL(IM::IP_MAX);
	tqLock.Lock();
	assert(thrd->waitTimeout);
	thrd->waitTimeout = 0;
	SleepEntry *se = (SleepEntry *)thrd->waitEntry;
	assert(se->numThreads);
	Wakeup(thrd);
	thrd->wakenByTimeout = 1;
	if (!se->numThreads) {
		FreeSleepEntry(se);
	}
	tqLock.Unlock();
	im->RestorePL(x);
}

int
PM::Sleep(waitid_t channelID, const char *sleepString, u64 timeout, int *byTimeout)
{
	u64 x = im->SetPL(IM::IP_MAX);
	tqLock.Lock();
	SleepEntry *se = GetSleepEntry(channelID);
	if (!se) {
		se = CreateSleepEntry(channelID);
	} else {
		if (se->flags & SleepEntry::F_WAKEN) {
			FreeSleepEntry(se);
			tqLock.Unlock();
			im->RestorePL(x);
			return 0;
		}
	}
	assert(se);
	Thread *thrd = Thread::GetCurrent();
	Runqueue *rq = thrd->GetRunqueue();
	rq->RemoveThread(thrd);
	LIST_ADD(sleepList, thrd, se->threads);
	se->numThreads++;
	Handle hTimeout;
	if (timeout) {
		hTimeout = tm->SetTimer(SleepTimeout, tm->GetTicks() + timeout, thrd);
		assert(hTimeout);
	} else {
		hTimeout = 0;
	}
	thrd->Sleep(se, sleepString, hTimeout);
	tqLock.Unlock();
	im->RestorePL(x);
	Thread *next = rq->SelectThread();
	/* at least idle thread should be there */
	assert(next);
	next->SwitchTo();
	if (byTimeout) {
		*byTimeout = thrd->wakenByTimeout;
	}
	return 0;
}

int
PM::ReserveSleepChannel(void *channelID)
{
	return ReserveSleepChannel((waitid_t)channelID);
}

int
PM::ReserveSleepChannel(waitid_t channelID)
{
	u64 x = im->SetPL(IM::IP_MAX);
	tqLock.Lock();
	SleepEntry *se = GetSleepEntry(channelID);
	if (!se) {
		se = CreateSleepEntry(channelID);
		assert(se);
	}
	tqLock.Unlock();
	im->RestorePL(x);
	return 0;
}

int
PM::FreeSleepChannel(void *channelID)
{
	return FreeSleepChannel((waitid_t)channelID);
}

int
PM::FreeSleepChannel(waitid_t channelID)
{
	u64 x = im->SetPL(IM::IP_MAX);
	tqLock.Lock();
	SleepEntry *se = GetSleepEntry(channelID);
	if (se && !se->numThreads) {
		FreeSleepEntry(se);
	}
	tqLock.Unlock();
	im->RestorePL(x);
	return 0;
}

int
PM::Sleep(void *channelID, const char *sleepString, u64 timeout, int *byTimeout)
{
	return Sleep((waitid_t)channelID, sleepString, timeout, byTimeout);
}

/* return 0 if someone waken, 1 if nobody waits on this channel, -1 if error */
int
PM::Wakeup(waitid_t channelID)
{
	/* prevent from deadlock in interrupts */
	u64 x = im->SetPL(IM::IP_MAX);
	tqLock.Lock();
	SleepEntry *se = GetSleepEntry(channelID);
	if (!se) {
		tqLock.Unlock();
		im->RestorePL(x);
		return 1;
	}
	int rc = Wakeup(se);
	tqLock.Unlock();
	im->RestorePL(x);
	return rc;
}

/* must be called with tqLock */
int
PM::Wakeup(SleepEntry *se)
{
	if (!se->numThreads) {
		/* it is reserved channel */
		se->flags |= SleepEntry::F_WAKEN;
		return 0;
	}
	Thread *thrd;
	while ((thrd = LIST_FIRST(Thread, sleepList, se->threads))) {
		Wakeup(thrd);
	}
	assert(!se->numThreads);
	FreeSleepEntry(se);
	return 0;
}

/* must be called with tqLock */
int
PM::Wakeup(Thread *thrd)
{
	SleepEntry *se = (SleepEntry *)thrd->waitEntry;
	assert(se);
	LIST_DELETE(sleepList, thrd, se->threads);
	assert(se->numThreads);
	se->numThreads--;
	if (thrd->waitTimeout) {
		tm->RemoveTimer(thrd->waitTimeout);
		thrd->waitTimeout = 0;
	}
	thrd->wakenByTimeout = 0;
	thrd->Unsleep();
	thrd->Run(); /* XXX CPU should be selected */
	return 0;
}

int
PM::Wakeup(void *channelID)
{
	return Wakeup((waitid_t)channelID);
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
	int pri;
	Thread *thrd;
	while (1) {
		if (!numActive) {
			/* start next round */
			Queue *x = qActive;
			qActive = qExpired;
			qExpired = x;
			numActive = numQueued;
			memcpy(activeMask, queuedMask, sizeof(activeMask));
		}
		pri = BitFirstSet(activeMask, sizeof(activeMask) * NBBY);
		assert(pri != -1 && pri < NUM_PRIORITIES);
		assert(!LIST_ISEMPTY(qActive->queue[pri]));
		if (pri != IDLE_PRIORITY) {
			break;
		}
		/* only idle thread should be left */
		if (numQueued == 1) {
			/* no more threads in running state */
			thrd = LIST_FIRST(Thread, rqList, qActive->queue[pri]);
			assert(thrd);
			return thrd;
		}
		assert(numActive == 1);
		numActive = 0;
		thrd = LIST_FIRST(Thread, rqList, qActive->queue[pri]);
		assert(thrd);
		LIST_DELETE(rqList, thrd, qActive->queue[pri]);
		LIST_ADD(rqList, thrd, qExpired->queue[pri]);
	}
	thrd = LIST_FIRST(Thread, rqList, qActive->queue[pri]);
	assert(thrd);
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
	//notimpl
	if (map) {
		mm->DestroyMap(map);
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

PM::Thread *
PM::Process::GetThread()
{
	return LIST_FIRST(Thread, list, threads);
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
/* ImageLoader class */

PM::ImageLoader::ImageLoader(VFS::File *file)
{
	file->AddRef();
	this->file = file;
}

PM::ImageLoader::~ImageLoader()
{
	file->Release();
}

/***********************************************/
/* ILRegistrator class */

PM::ILRegistrator::ILRegistrator(const char *desc, ILFactory factory, ILProber prober)
{
	PM::RegisterIL(desc, factory, prober);
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
	waitTimeout = 0;
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

ASM (
	".globl _OnThreadExit\n"
	"_OnThreadExit:\n"
	"addl	$4, %esp\n" /* pop argument for entry point */
	"pushl	%eax\n"
	"call	OnThreadExit\n");

extern "C" void _OnThreadExit();

void
PM::Thread::Exit(u32 exitCode)
{
	printf("Thread %d exited\n", GetID());//temp
	//notimpl
	while (1) {
		hlt();
	}
}

/* return 0 for caller, 1 for restored thread */
int
PM::Thread::SaveContext(Context *ctx)
{
	int rc;
	ASM (
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
		:
		: "cc"
	);
	return rc;
}

/* this method actually returns in SaveContext() */
void
PM::Thread::RestoreContext(Context *ctx, u32 asRoot)
{
	ASM (
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
		/* XXX should balance load */
		cpu = CPU::GetCurrent();
	}
	Runqueue *rq = (Runqueue *)cpu->pcpu.runQueue;
	return rq->AddThread(this);
}

int
PM::Thread::Stop()
{
	if (state != S_RUNNING) {
		return -1;
	}
	Runqueue *rq = GetRunqueue();
	assert(rq);
	rq->RemoveThread(this);
	return 0;
}

int
PM::Thread::Sleep(void *waitEntry, const char *waitString, Handle waitTimeout)
{
	assert(state == S_NONE);
	this->waitEntry = waitEntry;
	this->waitString = waitString;
	this->waitTimeout = waitTimeout;
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
	waitTimeout = 0;
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
		stackObj->Release();
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
