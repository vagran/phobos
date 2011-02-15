/*
 * /kernel/kern/pm.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <gate/gate.h>

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
	Runqueue *rq = CPU_RQ(cpu);
	assert(rq);
	sti();
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
	assert(LIST_ISEMPTY(p->threads));
	TREE_DELETE(tree, p, tqSleep);
	DELETE(p);
}

PM::Process *
PM::IntCreateProcess(Thread::ThreadEntry entry, void *arg, const char *name,
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
	if (proc->Initialize(priority, name, isKernelProc)) {
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
	if (entry) {
		if (!isKernelProc) {
			proc->mainThread = proc->CreateThread(entry, arg);
			if (!proc->mainThread) {
				DestroyProcess(proc);
				return 0;
			}
			if (runIt) {
				proc->mainThread->Run();
			}
		} else {
			kernInitThread = proc->CreateThread(entry, arg,
				Thread::DEF_KERNEL_STACK_SIZE, KERNEL_PRIORITY);
			ensure(kernInitThread);
			proc->mainThread = kernInitThread;
			if (runIt) {
				kernInitThread->Run();
			}
		}
	}
	return proc;
}

PM::Process *
PM::CreateProcess(Thread::ThreadEntry entry, void *arg, const char *name,
	int priority)
{
	return IntCreateProcess(entry, arg, name, priority);
}

PM::Process *
PM::CreateProcess(const char *path, const char *name, int priority, const char *args)
{
	if (!name) {
		name = path;
	}
	Process *proc = IntCreateProcess(0, 0, name, priority, 0, 0);
	if (!proc) {
		return 0;
	}
	proc->args = args;
	KString interp;
	KString sPath = path;
	ImageLoader *il;
	do {
		VFS::File *file = vfs->CreateFile(sPath.GetBuffer());
		if (!file) {
			DestroyProcess(proc);
			return 0;
		}
		il = GetImageLoader(file);
		file->Release();
		if (!il) {
			DestroyProcess(proc);
			return 0;
		}

		if (il->IsInterp(&interp)) {
			/*
			 * This is interpretable image, add current path to the process
			 * arguments and load interpreter.
			 */
			KString newArgs = sPath;
			newArgs += ' ';
			newArgs += proc->args;
			proc->args = newArgs;
			il->Release();
			sPath = interp;
			continue;
		}

		if (il->Load(proc->userMap, proc->bssObj)) {
			DestroyProcess(proc);
			il->Release();
			return 0;
		}
		break;
	} while (1);
	vaddr_t ep = il->GetEntryPoint();
	il->Release();

	Thread *thrd = proc->CreateUserThread(ep, Thread::DEF_STACK_SIZE, DEF_PRIORITY);
	if (!thrd) {
		DestroyProcess(proc);
		return 0;
	};
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
	ensure(CPU_RQ(cpu));
	rqLock.Lock();
	LIST_ADD(list, CPU_RQ(cpu), runQueues);
	rqLock.Unlock();
	/* The first caller should create kernel process */
	assert((!kernelProc && kernelProcEntry) || (kernelProc && !kernelProcEntry));
	if (kernelProcEntry) {
		/* create kernel process */
		kernelProc = IntCreateProcess(kernelProcEntry, arg, "[kernel]",
			KERNEL_PRIORITY, 1);
	}
	ensure(kernelProc);
	/* create idle thread */
	Thread *idleThread = kernelProc->CreateThread(IdleThread, this,
		Thread::DEF_KERNEL_STACK_SIZE, IDLE_PRIORITY);
	ensure(idleThread);
	idleThread->Run(cpu);
	cpu->pcpu.idleThread = idleThread;
	/* perform per-CPU gate management initialization */
	gm->InitCPU();
	/* switch to some thread */
	if (kernelProcEntry) {
		kernInitThread->SwitchTo();
	} else {
		idleThread->SwitchTo();
	}
	NotReached();
}

int
PM::ValidateState()
{
	CPU *cpu = CPU::GetCurrent();
	assert(cpu);
	Runqueue *rq = CPU_RQ(cpu);
	assert(rq);
	Thread *thrd = rq->GetCurrentThread();
	/* we are completed with a system call processing */
	thrd->gateCallSP = 0;
	thrd->gateCallRetAddr = 0;
	thrd->gateObj = 0;
	if (thrd->state != Thread::S_RUNNING || rq->needResched) {
		/* Currently active thread could be stopped, switch to another one */
		Thread *nextThrd = rq->SelectThread();
		/* at least idle thread should be there */
		assert(nextThrd);
		nextThrd->SwitchTo();
	}
	return 0;
}

void
PM::Tick(u32 ms)
{
	CPU *cpu = CPU::GetCurrent();
	if (!cpu) {
		return;
	}
	Runqueue *rq = CPU_RQ(cpu);
	assert(rq);
	Thread *thrd = rq->GetCurrentThread();
	assert(thrd);
	if (!thrd->rqQueue || thrd->isExpired) {
		return;
	}
	if (thrd->sliceTicks > ms) {
		thrd->sliceTicks -= ms;
	} else {
		thrd->sliceTicks = 0;
		rq->ExpireThread(thrd);
	}
}

CPU *
PM::SelectCPU()
{
	/* XXX should implement load balancing */
	return CPU::GetCurrent();
}

const char *
PM::StrProcessFault(ProcessFault flt)
{
	switch (flt) {
	case PFLT_NONE:
		return "No fault";
	case PFLT_GATE_OBJ:
		return "Invalid gate object passed to system call";
	case PFLT_GATE_OBJ_RELEASE:
		return "Gate object release called without matching AddRef()";
	case PFLT_GATE_METHOD:
		return "Invalid gate method called";
	case PFLT_GATE_STACK:
		return "Invalid stack pointer when called gate method";
	case PFLT_GATE_METHOD_RESTICTED:
		return "Restricted gate method called";
	case PFLT_PAGE_FAULT:
		return "Page fault";
	case PFLT_INVALID_BUFFER:
		return "Invalid user buffer provided to gate method";
	case PFLT_ABORT:
		return "Aborted by application request";
	case PFLT_THREAD_ARGS:
		return "Too many arguments provided to new thread";
	}
	return "Unknown fault";
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
	thrd->wakenBy = 0;
	WakeupThread(thrd);
	tqLock.Unlock();
	im->RestorePL(x);
}

int
PM::Sleep(waitid_t *channelsID, int numChannels,
		const char *sleepString, u64 timeout, waitid_t *wakenBy)
{
	if (numChannels == -1) {
		for (numChannels = 0; channelsID[numChannels]; numChannels++);
	}
	assert(numChannels);

	Thread *thrd = Thread::GetCurrent();
	u64 x = im->SetPL(IM::IP_MAX);
	tqLock.Lock();

	/* Allocate ThreadElement structures */
	ThreadElement *e;
	LIST_INIT(thrd->waitEntries);
	for (int i = 0; i < numChannels; i++) {
		e = NEW(ThreadElement);
		if (!e) {
			tqLock.Unlock();
			im->RestorePL(x);
			while ((e = LIST_FIRST(ThreadElement, thrdList, thrd->waitEntries))) {
				LIST_DELETE(thrdList, e, thrd->waitEntries);
				DELETE(e);
			}
			return -1;
		}
		LIST_ADD(thrdList, e, thrd->waitEntries);
		e->thread = thrd;
	}
	thrd->numWaitEntries = numChannels;

	/* Firstly check if there are no waken sleep entries */
	SleepEntry *se;
	int waken = 0;
	e = LIST_FIRST(ThreadElement, thrdList, thrd->waitEntries);
	for (int i = 0; i < numChannels; i++) {
		se = GetSleepEntry(channelsID[i]);
		e->se = se;
		if (se && (se->flags & SleepEntry::F_WAKEN)) {
			waken = 1;
			FreeSleepEntry(se);
			thrd->wakenBy = channelsID[i];
		}
		e = LIST_NEXT(ThreadElement, thrdList, e);
	}
	/* If at least one thread was waken then return */
	if (waken) {
		tqLock.Unlock();
		im->RestorePL(x);
		while ((e = LIST_FIRST(ThreadElement, thrdList, thrd->waitEntries))) {
			LIST_DELETE(thrdList, e, thrd->waitEntries);
			DELETE(e);
		}
		if (wakenBy) {
			*wakenBy = thrd->wakenBy;
		}
		return 0;
	}

	/*
	 * The next step, create non-existing sleep entries and add the thread
	 * to all of the sleep entries
	 */
	Runqueue *rq = thrd->GetRunqueue();
	rq->RemoveThread(thrd);
	e = LIST_FIRST(ThreadElement, thrdList, thrd->waitEntries);
	for (int i = 0; i < numChannels; i++) {
		if (e->se) {
			se = e->se;
		} else {
			se = CreateSleepEntry(channelsID[i]);
			e->se = se;
		}
		assert(se);

		LIST_ADD(seList, e, se->threads);
		se->numThreads++;
		e = LIST_NEXT(ThreadElement, thrdList, e);
	}

	/* Setup timer if wake up by timeout requested */
	Handle hTimeout;
	if (timeout) {
		hTimeout = tm->SetTimer(SleepTimeout, tm->GetTicks() + timeout, thrd);
		assert(hTimeout);
	} else {
		hTimeout = 0;
	}
	thrd->Sleep(sleepString, hTimeout);
	tqLock.Unlock();
	im->RestorePL(x);

	/* Switch to another thread */
	Thread *next = rq->SelectThread();
	/* at least idle thread should be there */
	assert(next);
	next->SwitchTo();
	/* We were waken up */
	if (wakenBy) {
		*wakenBy = thrd->wakenBy;
	}
	return 0;
}

int
PM::Sleep(void **channelsID, int numChannels,
			const char *sleepString, u64 timeout, void **wakenBy)
{
	return Sleep((waitid_t *)channelsID, numChannels, sleepString,
		timeout, (waitid_t *)wakenBy);
}

int
PM::Sleep(waitid_t channelID, const char *sleepString, u64 timeout, waitid_t *wakenBy)
{
	return Sleep(&channelID, 1, sleepString, timeout, wakenBy);
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
PM::Sleep(void *channelID, const char *sleepString, u64 timeout, void **wakenBy)
{
	return Sleep((waitid_t)channelID, sleepString, timeout, (waitid_t *)wakenBy);
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
	se->numThreads++; /* hold reference to sleep entry */
	ThreadElement *e;
	while ((e = LIST_FIRST(ThreadElement, seList, se->threads))) {
		Thread *thrd = e->thread;
		thrd->wakenBy = TREE_KEY(tree, se);
		WakeupThread(thrd);
	}
	assert(se->numThreads == 1);
	se->numThreads = 0;
	assert(LIST_ISEMPTY(se->threads));
	FreeSleepEntry(se);
	return 0;
}

/* must be called with tqLock */
int
PM::WakeupThread(Thread *thrd)
{
	UnsleepThread(thrd);
	thrd->Unsleep();
	thrd->Run();
	return 0;
}

/* must be called with tqLock */
int
PM::UnsleepThread(Thread *thrd)
{
	/* Remove all relations between thread and sleep entries */
	ThreadElement *e;
	while ((e = LIST_FIRST(ThreadElement, thrdList, thrd->waitEntries))) {
		SleepEntry *se = e->se;
		LIST_DELETE(seList, e, se->threads);
		assert(se->numThreads);
		se->numThreads--;
		if (!se->numThreads) {
			FreeSleepEntry(se);
		}
		LIST_DELETE(thrdList, e, thrd->waitEntries);
		assert(thrd->numWaitEntries);
		thrd->numWaitEntries--;
		DELETE(e);
	}

	/* Destroy timer if it was created */
	if (thrd->waitTimeout) {
		tm->RemoveTimer(thrd->waitTimeout);
		thrd->waitTimeout = 0;
	}
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
	needResched = 0;
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
	u32 intr = IM::DisableIntr();
	qLock.Lock();
	thrd->rqQueue = qActive;
	thrd->cpu = cpu;
	LIST_ADD_LAST(rqList, thrd, qActive->queue[thrd->priority]);
	BitSet(activeMask, thrd->priority);
	BitSet(queuedMask, thrd->priority);
	numQueued++;
	numActive++;
	needResched = 1;
	qLock.Unlock();
	IM::RestoreIntr(intr);
	return 0;
}

int
PM::Runqueue::RemoveThread(Thread *thrd)
{
	assert(CPU_RQ(thrd->cpu) == this);
	u32 intr = IM::DisableIntr();
	qLock.Lock();
	assert(thrd->rqQueue);
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
	thrd->state = Thread::S_NONE;
	thrd->priority = thrd->nextPriority;
	thrd->isActive = 0;
	thrd->rqQueue = 0;
	needResched = 1;
	qLock.Unlock();
	IM::RestoreIntr(intr);
	return 0;
}

int
PM::Runqueue::ExpireThread(Thread *thrd)
{
	u32 intr = IM::DisableIntr();
	qLock.Lock();
	assert(!thrd->isExpired);
	thrd->isExpired = 1;
	assert(thrd->rqQueue == qActive);
	ListHead *head = &qActive->queue[thrd->priority];
	LIST_DELETE(rqList, thrd, *head);
	if (LIST_ISEMPTY(*head)) {
		BitClear(activeMask, thrd->priority);
	}
	assert(numActive);
	numActive--;
	if (thrd->priority != thrd->nextPriority) {
		if (LIST_ISEMPTY(qActive->queue[thrd->priority]) &&
			LIST_ISEMPTY(qExpired->queue[thrd->priority])) {
			BitClear(queuedMask, thrd->priority);
		}
		thrd->priority = thrd->nextPriority;
		BitSet(queuedMask, thrd->priority);
	}
	thrd->sliceTicks = GetSliceTicks(thrd);
	head = &qExpired->queue[thrd->priority];
	LIST_ADD(rqList, thrd, *head);
	thrd->rqQueue = qExpired;
	needResched = 1;
	qLock.Unlock();
	IM::RestoreIntr(intr);
	return 0;
}

PM::Thread *
PM::Runqueue::SelectThread()
{
	assert(numQueued);
	int pri;
	Thread *thrd;

	u32 intr = IM::DisableIntr();
	qLock.Lock();
	needResched = 0;
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
			/* no more threads in running state, just idle thread */
			thrd = LIST_FIRST(Thread, rqList, qActive->queue[pri]);
			assert(thrd);
			thrd->isExpired = 1; /* prevent from expiration */
			qLock.Unlock();
			IM::RestoreIntr(intr);
			return thrd;
		}
		/*
		 * There are other threads in runqueue, expire idle thread in order to
		 * start next round.
		 */
		assert(numActive == 1);
		numActive = 0;
		thrd = LIST_FIRST(Thread, rqList, qActive->queue[pri]);
		assert(thrd);
		LIST_DELETE(rqList, thrd, qActive->queue[pri]);
		LIST_ADD(rqList, thrd, qExpired->queue[pri]);
		thrd->rqQueue = qExpired;
	}
	thrd = LIST_FIRST(Thread, rqList, qActive->queue[pri]);
	assert(thrd);
	assert(thrd->rqQueue == qActive);
	qLock.Unlock();
	IM::RestoreIntr(intr);
	return thrd;
}

/***********************************************/
/* Process class */

PM::Process::Process()
{
	LIST_INIT(threads);
	STREE_INIT(streams);
	numThreads = 0;
	numAliveThreads = 0;
	map = 0;
	userMap = 0;
	gateMap = 0;
	gateArea = 0;
	heapObj = 0;
	isKernelProc = 0;
	mainThread = 0;
	fault = PFLT_NONE;
	state = S_STOPPED;
}

PM::Process::~Process()
{
	/* destroy threads */
	while (1) {
		thrdListLock.Lock();
		Thread *t = LIST_FIRST(Thread, list, threads);
		if (!t) {
			thrdListLock.Unlock();
			break;
		}
		LIST_DELETE(list, t, threads);
		numThreads--;
		thrdListLock.Unlock();
		DeleteThread(t);
	}
	if (gateArea) {
		DELETE(gateArea);
	}
	if (bssObj) {
		bssObj->Release();
	}
	if (heapObj) {
		heapObj->Release();
	}
	if (map) {
		mm->DestroyMap(map);
	}
}

int
PM::Process::DeleteThread(Thread *t)
{
	assert(t != Thread::GetCurrent());
	t->Stop();
	DELETE(t);
	return 0;
}

int
PM::Process::TerminateThread(Thread *thrd, int exitCode)
{
	assert(thrd->GetProcess() == this);
	thrd->Terminate(exitCode);
	thrdListLock.Lock();
	numAliveThreads--;
	thrdListLock.Unlock();
	if (thrd == mainThread) {
		Terminate();
	}
	return 0;
}

PM::Thread *
PM::Process::CreateThread(Thread::ThreadEntry entry, void *arg, u32 stackSize,
	u32 priority)
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
	if (!mainThread) {
		mainThread = thrd;
	}
	LIST_ADD(list, thrd, threads);
	numThreads++;
	numAliveThreads++;
	thrdListLock.Unlock();
	return thrd;
}

int
PM::Process::UserThreadEntry(UserThreadParams *params)
{
	/*
	 * We will overwrite current stack bottom. So we need special actions in
	 * order to use local variables and argument - we will move our frame
	 * upper to the stack.
	 */
	const u32 frameShift = params->numArgs * sizeof(u32) + 256;

	if (frameShift > 8192) {
		Thread::GetCurrent()->Fault(PFLT_THREAD_ARGS);
		pm->ValidateState();
		NotReached();
	}
	memcpy((u8 *)&params - frameShift, &params, sizeof(UserThreadParams *));
	ASM (
		"subl %0, %%ebp\n"
		"subl %0, %%esp\n"
		:
		: "r"(frameShift)
		: "cc", "memory"
	);
	sti();

	/* New block to ensure all local variables are allocated from here */
	{
		Thread *thrd = Thread::GetCurrent();
		u32 *esp = (u32 *)(thrd->stackEntry->base + thrd->stackEntry->size);
		/*
		 * Prepare stack. Push all arguments in reverse order (C calling convention).
		 * Initialize gating SS and pass pointer to GApp if it is main thread
		 * creation.
		 */
		Process *proc = thrd->GetProcess();
		if (proc->GetThread() == thrd) {
			proc->gateArea->Initialize();
			GApp *app = proc->gateArea->GetApp();
			app->AddRef(); /* bump user reference counter */
			*(--esp) = (u32)app;

			/* Create default streams */
			/* XXX just temporal stub, provide terminal access for process */
			ConsoleDev *videoTerm = (ConsoleDev *)devMan.GetDevice("syscons"/*//temp "vga" */, 0);
			GNEW(proc->gateArea, GConsoleStream, "output", videoTerm, GConsoleStream::F_OUTPUT);
			GNEW(proc->gateArea, GConsoleStream, "trace", videoTerm, GConsoleStream::F_OUTPUT);
			GNEW(proc->gateArea, GConsoleStream, "log", videoTerm, GConsoleStream::F_OUTPUT);
			GNEW(proc->gateArea, GConsoleStream, "input",
				(ConsoleDev *)devMan.GetDevice("kbdcons", 0), GConsoleStream::F_INPUT);
		} else {
			for (int i = params->numArgs - 1; i >= 0; i--) {
				*(--esp) = params->args[i];
			}
		}
		/* push fake return address for debugger */
		*(--esp) = 0;

		vaddr_t entry = params->entry;
		MM::mfree(params);
		ASM (
			"movl	%0, %%ds\n" /* switch to user data segments */
			"movl	%0, %%es\n"
			"xorl	%%ebp, %%ebp\n" /* zero frame base for debugger */
			"xorl	%0, %0\n"
			"mov	%0, %%fs\n"
			"mov	%0, %%gs\n"
			"sysexit\n" /* jump to user land */
			:
			: "r"((u32)GDT::GetSelector(GDT::SI_UDATA, GDT::PL_USER)),
			  "d"(entry), "c"(esp)
			:
		);
		NotReached();
	}
}

PM::Thread *
PM::Process::CreateUserThread(vaddr_t entry, u32 stackSize, u32 priority,
	u32 numArgs, ...)
{
	u32 paramSize = OFFSETOF(UserThreadParams, args) + numArgs * sizeof(u32);
	UserThreadParams *params = (UserThreadParams *)MM::malloc(paramSize);
	if (!params) {
		ERROR(E_NOMEM, "Cannot allocate user thread parameters");
		return 0;
	}
	memset(params, 0, paramSize);
	params->entry = entry;
	params->numArgs = numArgs;
	va_list args;
	va_start(args, numArgs);
	for (u32 i = 0; i < numArgs; i++) {
		params->args[i] = va_arg(args, u32);
	}
	return CreateThread((Thread::ThreadEntry)UserThreadEntry, params,
		stackSize, priority);
}

PM::Thread *
PM::Process::GetThread()
{
	return mainThread;
}

int
PM::Process::AddStream(GStream *stream)
{
	streamsLock.Lock();
	if (STREE_FIND(GStream, nameTree, streams, stream->streamName)) {
		streamsLock.Unlock();
		return -1;
	}
	STREE_ADD(nameTree, stream, streams, stream->streamName);
	streamsLock.Unlock();
	return 0;
}

int
PM::Process::RemoveStream(GStream *stream)
{
	streamsLock.Lock();
	STREE_DELETE(nameTree, stream, streams);
	streamsLock.Unlock();
	return 0;
}

GStream *
PM::Process::GetStream(const char *name)
{
	GStream *stream;
	streamsLock.Lock();
	stream = STREE_FIND(GStream, nameTree, streams, (char *)name);
	streamsLock.Unlock();
	return stream;
}

/* Check if the buffer provided from the user space is valid  */
int
PM::Process::CheckUserBuf(void *buf, u32 size, MM::Protection protection)
{
	if (!size) {
		return 0;
	}

	PM::Thread *thrd = PM::Thread::GetCurrent();
	PM::Process *proc = thrd->GetProcess();
	MM::Map *map = proc->GetUserMap();

	/* protect kernel stack, deny any access */
	if (thrd->IsKernelStack((vaddr_t)buf, size)) {
		thrd->Fault(PM::PFLT_INVALID_BUFFER,
			"Invalid buffer - %lu bytes at 0x%08lx, "
			"access denied to the kernel stack, requested access: %c%c%c",
			size, (u32)buf,
			(protection & MM::PROT_READ) ? 'r' : '-',
			(protection & MM::PROT_WRITE) ? 'w' : '-',
			(protection & MM::PROT_EXEC) ? 'x' : '-');
		return -1;
	}

	vaddr_t endVa = roundup2((vaddr_t)buf + size, PAGE_SIZE);
	for (vaddr_t va = rounddown2((vaddr_t)buf, PAGE_SIZE); va < endVa;
		va += PAGE_SIZE) {
		if (map->CheckPageProtection(va, protection, 1)) {
			thrd->Fault(PM::PFLT_INVALID_BUFFER,
				"Invalid buffer - %lu bytes at 0x%08lx, failed for page 0x%08lx, "
				"requested access: %c%c%c", size, (u32)buf, va,
				(protection & MM::PROT_READ) ? 'r' : '-',
				(protection & MM::PROT_WRITE) ? 'w' : '-',
				(protection & MM::PROT_EXEC) ? 'x' : '-');
			return -1;
 		}
	}
	return 0;
}

/* Check if the string pointer provided from the user space is valid */
int
PM::Process::CheckUserString(const char *str)
{
	PM::Thread *thrd = PM::Thread::GetCurrent();
	PM::Process *proc = thrd->GetProcess();
	MM::Map *map = proc->GetUserMap();

	/* scan pages for zero one by one */
	vaddr_t endVa = roundup2((vaddr_t)str + 1, PAGE_SIZE);
	vaddr_t va = (vaddr_t)str;
	while (1) {
		/* protect the kernel stack */
		if (thrd->IsKernelStack(va, endVa - va)) {
			thrd->Fault(PM::PFLT_INVALID_BUFFER, "Invalid string at 0x%08lx, "
				"failed for page 0x%08lx, kernel stack access denied",
				(u32)str, va);
			return -1;
		}

		if (map->CheckPageProtection(va, MM::PROT_READ, 1)) {
			thrd->Fault(PM::PFLT_INVALID_BUFFER, "Invalid string at 0x%08lx, "
				"failed for page 0x%08lx", (u32)str, va);
			return -1;
		}
		while (va < endVa) {
			if (!*(char *)va) {
				return 0;
			}
			va++;
		}
		endVa += PAGE_SIZE;
	}
}

void
PM::Process::SetState(State state)
{
	int changed = this->state != state;
	this->state = state;
	if (changed) {
		pm->Wakeup(this);
	}
}

int
PM::Process::Stop()
{
	Thread *thrd;
	thrdListLock.Lock();
	LIST_FOREACH(Thread, list, thrd, threads) {
		if (thrd->state != Thread::S_TERMINATED) {
			thrd->Stop();
		}
	}
	thrdListLock.Unlock();
	SetState(S_STOPPED);
	return 0;
}

int
PM::Process::Resume()
{
	if (state != S_STOPPED) {
		return -1;
	}
	SetState(S_RUNNING);
	Thread *thrd;
	thrdListLock.Lock();
	LIST_FOREACH(Thread, list, thrd, threads) {
		if (thrd->state != Thread::S_TERMINATED) {
			thrd->Run();
		}
	}
	thrdListLock.Unlock();
	return 0;
}

int
PM::Process::Terminate()
{
	Stop();
	SetState(S_TERMINATED);
	return 0;
}

int
PM::Process::Fault(ProcessFault flt, const char *msg, ...)
{
	fault = flt;
	if (msg) {
		va_list args;
		va_start(args, msg);
		faultStr.FormatV(msg, args);
	}
	Terminate();
	klog(KLOG_WARNING, "Process %u (%s) failed: %s", GetID(), name.GetBuffer(),
		faultStr.GetBuffer());
	return 0;
}

int
PM::Process::Initialize(u32 priority, const char *name, int isKernelProc)
{
	this->name = name;
	this->isKernelProc = isKernelProc;
	/* Create gate area map if it is not kernel process */
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
		gateMap->isUser = 1;
	}

	/* Create user space map */
	userMap = map->CreateSubmap(0, GATE_AREA_ADDRESS);
	if (!userMap) {
		return -1;
	}
	userMap->isUser = 1;

	/*
	 * We do not want valid zero pointers so restrict allocation of the very
	 * first page - create not mapped guard page at the very bottom of the
	 * process virtual address space.
	 */
	ensure(userMap->ReserveSpace(0, PAGE_SIZE));

	/*
	 * BSS object is used for BSS binary sections which must be initialized to
	 * zeros, and for user heap allocations which also must be zeroed.
	 */
	bssObj = NEW(MM::VMObject, GATE_AREA_ADDRESS,
		MM::VMObject::F_HEAP | MM::VMObject::F_ZERO);
	if (!bssObj) {
		return -1;
	}

	/* Heap object is used for dynamic user space memory allocations */
	heapObj = NEW(MM::VMObject, GATE_AREA_ADDRESS, MM::VMObject::F_HEAP);
	if (!heapObj) {
		return -1;
	}

	/* Create gate area object for non-kernel processes */
	if (!isKernelProc) {
		gateArea = gm->CreateGateArea(this);
	}

	state = S_RUNNING;
	return 0;
}

void *
PM::Process::AllocateHeap(u32 size, int prot, void *location, int getZeroed)
{
	if (prot & ~(MM::PROT_READ | MM::PROT_WRITE | MM::PROT_EXEC)) {
		ERROR(E_INVAL, "Invalid protection value specified");
		return 0;
	}
	MM::Map::Entry *e;
	MM::VMObject *obj = getZeroed ? bssObj : heapObj;
	if (location) {
		e = userMap->InsertObjectAt(obj, (vaddr_t)location, (vaddr_t)location,
			size, prot);
	} else {
		e = userMap->InsertObjectOffs(obj, size, 0, prot);
	}
	if (!e) {
		ERROR(E_FAULT, "Failed to allocate entry for heap object");
		return 0;
	}
	return (void *)e->base;
}

MM::Map::Entry *
PM::Process::GetHeapEntry(vaddr_t va)
{
	/* Try to find corresponding entry in user map and check it belongs to heap object */
	MM::Map::Entry *e = userMap->Lookup(va, 0);
	if (!e) {
		ERROR(E_INVAL, "Heap entry not found for specified address (0x%08lx)", va);
		return 0;
	}
	if (e->object != heapObj && e->object != bssObj) {
		ERROR(E_INVAL, "The entry doesn't belong to heap object (0x%08lx)", va);
		return 0;
	}
	return e;
}

int
PM::Process::FreeHeap(void *p)
{
	MM::Map::Entry *e = GetHeapEntry((vaddr_t)p);
	if (!e) {
		ERROR(E_FAULT, "Cannot find heap entry");
		return -1;
	}
	if (userMap->Free(e)) {
		ERROR(E_FAULT, "Map allocator Free() method failed");
		return -1;
	}
	return 0;
}

void *
PM::Process::ReserveSpace(u32 size, vaddr_t va)
{
	MM::Map::Entry *e = userMap->AllocateSpace(size, &va, va != 0);
	if (!e) {
		ERROR(E_NOMEM, "Cannot allocate space in the user map (0x%lx @ %08lx)",
			size, va);
		return 0;
	}
	e->flags |= MM::Map::Entry::F_USERRESERVED;
	return (void *)e->base;
}

int
PM::Process::UnReserveSpace(void *p)
{
	MM::Map::Entry *e = userMap->Lookup((vaddr_t)p, 0);
	if (!e) {
		ERROR(E_INVAL, "Map entry not found for specified address (0x%08lx)",
			(vaddr_t)p);
		return -1;
	}
	if (!(e->flags & MM::Map::Entry::F_USERRESERVED)) {
		ERROR(E_INVAL, "Map entry is not space reservation entry (0x%08lx)",
			(vaddr_t)p);
		return -1;
	}
	if (userMap->Free(e)) {
		ERROR(E_FAULT, "Map allocator Free() method failed");
		return -1;
	}
	return 0;
}

u32
PM::Process::GetHeapSize(void *p)
{
	MM::Map::Entry *e = GetHeapEntry((vaddr_t)p);
	if (!e) {
		ERROR(E_FAULT, "Cannot find heap entry");
		return 0;
	}
	return e->size;
}

PM::Process::State
PM::Process::GetState(u32 *pExitCode, ProcessFault *pFault)
{
	if (pExitCode) {
		*pExitCode = mainThread->exitCode;
	}
	if (pFault) {
		*pFault = fault;
	}
	return state;
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
	isActive = 0;
	exitCode = 0;
	fault = PFLT_NONE;
	memset(&ctx, 0, sizeof(ctx));
	priority = DEF_PRIORITY;
	nextPriority = priority;
	LIST_INIT(waitEntries);
	numWaitEntries = 0;
	waitString = 0;
	waitTimeout = 0;
	rqQueue = 0;
	isExpired = 0;
	curError = 0;
	numErrors = 0;
	gateObj = 0;
	gateCallSP = 0;
	gateCallRetAddr = 0;
	gateCallMode = 0;
	activationTime = 0;
	runTime = 0;
	kstackPDE.raw = 0;
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
PM::Thread::Exit(int exitCode)
{
	assert(GetCurrent() == this);
	proc->TerminateThread(this, exitCode);

	/* switch to another thread */
	CPU *cpu = CPU::GetCurrent();
	assert(cpu);
	Runqueue *rq = CPU_RQ(cpu);
	assert(rq);
	Thread *nextThrd = rq->SelectThread();
	/* at least idle thread should be there */
	assert(nextThrd);
	nextThrd->SwitchTo();
	NotReached();
}

PM::Thread *
PM::Thread::GetCurrent()
{
	CPU *cpu = CPU::GetCurrent();
	if (!cpu) {
		return 0;
	}
	Runqueue *rq = CPU_RQ(cpu);
	assert(rq);
	return rq->GetCurrentThread();
}

int
PM::Thread::Run(CPU *cpu)
{
	assert(state == S_NONE || state == S_STOPPED);
	if (this->cpu) {
		/*
		 * We are currently assigned to some CPU so we cannot change it
		 * (e.g. the thread was removed from its run-queue but the context
		 * was not yet switched when it was added back again).
		 */
		if (cpu) {
			ensure(cpu == this->cpu);
		} else {
			cpu = this->cpu;
		}
	} else if (!cpu) {
		/* do load balancing */
		cpu = pm->SelectCPU();
	}
	assert(cpu);
	Runqueue *rq = CPU_RQ(cpu);
	return rq->AddThread(this);
}

int
PM::Thread::Stop()
{
	if (state == S_STOPPED) {
		return 0;
	}
	if (state == S_SLEEP) {
		pm->tqLock.Lock();
		pm->UnsleepThread(this);
		pm->tqLock.Unlock();
		Unsleep();
	} else {
		assert(state == S_RUNNING);
		Runqueue *rq = GetRunqueue();
		assert(rq);
		CPU *cpu = this->cpu;
		rq->RemoveThread(this); /* state is S_NONE now */
		if (isActive) {
			if (this != rq->GetCurrentThread()) {
				/* Thread is currently active on some another CPU */
				cpu->DeactivateThread();
			}
		}
	}
	state = S_STOPPED;
	pm->Wakeup((waitid_t)this);
	return 0;
}

int
PM::Thread::Terminate(int exitCode)
{
	Stop();
	state = S_TERMINATED;
	this->exitCode = exitCode;
	pm->Wakeup((waitid_t)this);
	return 0;
}

int
PM::Thread::Sleep(const char *waitString, Handle waitTimeout)
{
	assert(state == S_NONE);
	this->waitString = waitString;
	this->waitTimeout = waitTimeout;
	state = S_SLEEP;
	return 0;
}

int
PM::Thread::Unsleep()
{
	assert(state == S_SLEEP);
	assert(LIST_ISEMPTY(waitEntries));
	state = S_NONE;
	waitString = 0;
	waitTimeout = 0;
	return 0;
}

void
PM::Thread::SwapContext(Context *ctx)
{
	ASM (
		"xchgl	%%esp, (%0)\n"
		"xchgl	%%ebp, 0x4(%0)\n"
		/* Switch kernel stack */
		"movl	(%1), %%eax\n"
		"xchgl	%%eax, 0x14(%0)\n"
		"movl	%%eax, (%1)\n"
		"movl	0x4(%1), %%eax\n"
		"xchgl	%%eax, 0x18(%0)\n"
		"movl	%%eax, 0x4(%1)\n"
		/* Exchange CR3 */
		"movl	%%cr3, %%eax\n"
		"xchgl	%%eax, 0x8(%0)\n"
		"movl	%%eax, %%cr3\n"
		/* Exchange EIP */
		"movl	$1f, %%eax\n"
		"xchgl	%%eax, 0x10(%0)\n"
		/*
		 * We will do "ret" on this address later. Whole state should be saved
		 * BEFORE we will potentially enable interrupts by restoring EFLAGS.
		 */
		"pushl	%%eax\n"
		/* Exchange EFLAGS */
		"pushfl\n"
		"popl	%%eax\n"
		"xchgl	%%eax, 0xc(%0)\n"
		"pushl	%%eax\n"
		"popfl\n" /* Restore EFLAGS ... */
		"ret\n" /* ... and EIP */
		"1:\n"
		:
		: "r"(ctx), "r"(proc->map->GetPDE(KSTACK_ADDRESS))
		: "eax"
	);
}

/* thread must be in the same CPU runqueue with the current thread */
void
PM::Thread::SwitchTo()
{
	assert(state == S_RUNNING);
	assert(cpu == CPU::GetCurrent());

	Thread *prev = GetCurrent();
	if (prev == this) {
		isExpired = 0;
		return;
	}
	u32 intr = IM::DisableIntr();

	Context *swapCtx;
	if (prev) {
		prev->isActive = 0;
		prev->isExpired = 0;
		prev->runTime += rdtsc() - prev->activationTime;
		if (!prev->rqQueue) {
			/* It was removed from run-queue */
			prev->cpu = 0;
		}
		prev->ctx = ctx;
		swapCtx = &prev->ctx;
	} else {
		swapCtx = &ctx;
	}

	/* Switch thread kernel stack */
	swapCtx->kstackPDE = kstackPDE.raw;

	/*
	 * Switch address space, even in case it is the same process we write to
	 * CR3 register in order to update thread kernel stack mapping
	 */
	swapCtx->cr3 = proc->map->GetCR3();

	Runqueue *rq = GetRunqueue();
	rq->curThread = this;
	isActive = 1;
	activationTime = rdtsc();
	SwapContext(swapCtx);
	IM::RestoreIntr(intr);
}

int
PM::Thread::CreateKstack()
{
	/* Allocate separate page-table */
	MM::Page *pt = mm->AllocatePage();
	if (!pt) {
		return -1;
	}
	PTE::PTEntry *pt_va = (PTE::PTEntry *)mm->QuickMapEnter(pt->pa);
	memset(pt_va, 0, PAGE_SIZE);
	int numPages = KSTACK_PHYS_SIZE / PAGE_SIZE;
	/* Allocate and enter all pages of the stack */
	PTE::PTEntry *pte = pt_va + (PT_ENTRIES - numPages);
	while (numPages) {
		MM::Page *pg = mm->AllocatePage();
		if (!pg) {
			break;
		}
		pte->fields.physAddr = pg->pa >> PAGE_SHIFT;
		pte->fields.present = 1;
		pte->fields.write = 1;
		numPages--;
		pte++;
	}
	if (numPages) {
		/* Allocation failure */
		while (pte > pt_va) {
			pte--;
			mm->FreePage(mm->GetPage(pte->fields.physAddr << PAGE_SHIFT));
		}
		mm->QuickMapRemove((vaddr_t)pt_va);
		mm->FreePage(pt);
		return -1;
	}
	mm->QuickMapRemove((vaddr_t)pt_va);
	kstackPDE.fields.physAddr = pt->pa >> PAGE_SHIFT;
	kstackPDE.fields.present = 1;
	kstackPDE.fields.write = 1;
	return 0;
}

void
PM::Thread::DestroyKstack()
{
	if (!kstackPDE.fields.present) {
		return;
	}
	MM::Page *pt = mm->GetPage(kstackPDE.fields.physAddr << PAGE_SHIFT);
	PTE::PTEntry *pt_va = (PTE::PTEntry *)mm->QuickMapEnter(pt->pa);
	PTE::PTEntry *pte = pt_va + (KSTACK_PHYS_SIZE / PAGE_SIZE);
	while (pte > pt_va) {
		pte--;
		mm->FreePage(mm->GetPage(pte->fields.physAddr << PAGE_SHIFT));
	}
	mm->QuickMapRemove((vaddr_t)pt_va);
	mm->FreePage(pt);
	kstackPDE.raw = 0;
}

int
PM::Thread::Initialize(ThreadEntry entry, void *arg, u32 stackSize, u32 priority)
{
	this->stackSize = stackSize;
	this->priority = priority;
	this->nextPriority = priority;
	if (CreateKstack()) {
		return -1;
	}
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
	ctx.eflags = GetEflags() & ~EFLAGS_IF;
	return 0;
}

/* assure the kernel will have required amount of stack space */
int
PM::Thread::MapKernelStack(vaddr_t esp)
{
	assert(IsValidSP(esp));
	vaddr_t startVa = rounddown2(esp - CPU::DEF_KERNEL_STACK_SIZE, PAGE_SIZE);
	if (startVa < stackEntry->base) {
		return -1;
	}
	vaddr_t endVa = rounddown2(esp, PAGE_SIZE);
	for (vaddr_t va = startVa; va < endVa; va += PAGE_SIZE) {
		if (proc->userMap->IsMapped(va)) {
			continue;
		}
		if (proc->userMap->Pagein(va, 1)) {
			return -1;
		}
	}
	return 0;
}

int
PM::Thread::IsKernelStack(vaddr_t addr, vsize_t size)
{
	vaddr_t stackStart = stackEntry->base;
	vaddr_t stackEnd = gateCallSP + sizeof(vaddr_t);
	vaddr_t endAddr = addr + size;

	/*
	 * Check for intersection with the kernel stack,
	 * keep in mind the address space wrapping.
	 */
	if (endAddr <= stackStart &&
		(addr < endAddr || addr >= stackEnd)) {
		return 0;
	}
	if (addr >= stackEnd &&
		(endAddr > addr || endAddr <= stackStart)) {
		return 0;
	}
	return 1;
}

PM::Thread::~Thread()
{
	pm->procListLock.Lock();
	TREE_DELETE(tree, &pid, pm->pids);
	pm->procListLock.Unlock();
	pm->ReleasePID(GetID());

	if (stackEntry) {
		proc->userMap->Free(stackEntry);
		stackEntry = 0;
	}

	if (stackObj) {
		stackObj->Release();
		stackObj = 0;
	}
	DestroyKstack();
}

Error *
PM::Thread::GetError(int depth)
{
	if (depth < 0) {
		return 0;
	}
	if (depth + 1 > numErrors) {
		/* we do not have required error object in the history */
		return 0;
	}
	int idx = curError;
	while (depth) {
		idx++;
		if (idx == ERROR_HISTORY_SIZE) {
			idx = 0;
		}
		depth--;
	}
	return &err[idx];
}

/* Allocate new error object */
Error *
PM::Thread::NextError()
{
	if (numErrors) {
		curError++;
		if (curError == ERROR_HISTORY_SIZE) {
			curError = 0;
		}
		if (numErrors < ERROR_HISTORY_SIZE) {
			numErrors++;
		}
	} else {
		numErrors = 1;
		curError = 0;
	}
	err[curError].Reset();
	return &err[curError];
}

/* Switch to previous error object, do not reset it */
Error *
PM::Thread::PrevError()
{
	if (!numErrors) {
		return 0;
	}
	err[curError].Reset();
	numErrors--;
	if (!curError) {
		curError = ERROR_HISTORY_SIZE - 1;
	} else {
		curError--;
	}
	return &err[curError];
}

int
PM::Thread::Fault(ProcessFault flt, const char *msg, ...)
{
	fault = flt;
	if (msg) {
		va_list args;
		va_start(args, msg);
		faultStr.FormatV(msg, args);
	}
	ERROR(E_THREAD_FAULT, "%s: %s", PM::StrProcessFault(flt), faultStr.GetBuffer());
	klog(KLOG_WARNING, "Thread %u (process %u) failed at 0x%08lx: %s: %s",
		GetID(), proc->GetID(),
		gateCallRetAddr ? gateCallRetAddr : (trapFrame ? trapFrame->eip : 0),
		PM::StrProcessFault(flt), faultStr.GetBuffer());
	switch (flt) {
	default:
		proc->Fault(flt, "Fault \"%s\" occurred in thread %u",
			PM::StrProcessFault(flt), GetID());
	}
	return 0;
}

PM::Thread::State
PM::Thread::GetState(u32 *pExitCode, ProcessFault *pFault)
{
	if (pExitCode) {
		*pExitCode = exitCode;
	}
	if (pFault) {
		*pFault = fault;
	}
	return state;
}
