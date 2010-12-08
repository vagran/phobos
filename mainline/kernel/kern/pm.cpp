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
			Thread *thrd = proc->CreateThread(entry, arg);
			if (!thrd) {
				DestroyProcess(proc);
				return 0;
			}
			if (runIt) {
				thrd->Run();
			}
		} else {
			kernInitThread = proc->CreateThread(entry, arg,
				Thread::DEF_KERNEL_STACK_SIZE, KERNEL_PRIORITY);
			ensure(kernInitThread);
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

int
PM::ProcessEntry(void *arg)
{
	Thread *thrd = Thread::GetCurrent();
	Process *proc = thrd->GetProcess();

	/* we are overwriting current stack bottom */
	u32 *esp = (u32 *)(thrd->stackEntry->base + thrd->stackEntry->size);
	/*
	 * Prepare stack. The only argument for AppStart() and Main() functions of
	 * an application is a pointer to GApp object in its gate area.
	 */
	proc->gateArea->Initialize();
	GApp *app = proc->gateArea->GetApp();
	app->AddRef(); /* bump user reference counter */

	/* Create default streams */
	/* XXX just temporal stub, provide terminal access for process */
	ConsoleDev *videoTerm = (ConsoleDev *)devMan.GetDevice("syscons"/*//temp "vga" */, 0);
	GNEW(proc->gateArea, GConsoleStream, "output", videoTerm, GConsoleStream::F_OUTPUT);
	GNEW(proc->gateArea, GConsoleStream, "trace", videoTerm, GConsoleStream::F_OUTPUT);
	GNEW(proc->gateArea, GConsoleStream, "log", videoTerm, GConsoleStream::F_OUTPUT);
	GNEW(proc->gateArea, GConsoleStream, "input",
		(ConsoleDev *)devMan.GetDevice("kbdcons", 0), GConsoleStream::F_INPUT);

	*(--esp) = (u32)app;
	/* push fake return address for debugger */
	*(--esp) = 0;
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
		  "d"(proc->entryPoint), "c"(esp)
		:
	);
	NotReached();
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
	proc->SetEntryPoint(il->GetEntryPoint());
	il->Release();
	Thread *thrd = proc->CreateThread(ProcessEntry, (void *)args);
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
	idleThread->Run();
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
	Thread *thrd = Thread::GetCurrent();
	/* we are completed with a system call processing */
	thrd->gateCallSP = 0;
	thrd->gateCallRetAddr = 0;
	thrd->gateObj = 0;

	if (thrd->state != Thread::S_RUNNING) {
		/* Currently active thread should be stopped, switch to another one */
		CPU *cpu = CPU::GetCurrent();
		assert(cpu);
		Runqueue *rq = CPU_RQ(cpu);
		assert(rq);
		Thread *nextThrd = rq->SelectThread();
		/* at least idle thread should be there */
		assert(nextThrd);
		nextThrd->SwitchTo();
	}
	return 0;
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
	Wakeup(thrd);
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
		Wakeup(thrd);
	}
	assert(se->numThreads == 1);
	se->numThreads = 0;
	assert(LIST_ISEMPTY(se->threads));
	FreeSleepEntry(se);
	return 0;
}

/* must be called with tqLock */
int
PM::Wakeup(Thread *thrd)
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
			qLock.Unlock();
			IM::RestoreIntr(intr);
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
	if (!numAliveThreads) {
		Stop();
		state = S_TERMINATED;
	}
	return 0;
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
	numAliveThreads++;
	thrdListLock.Unlock();
	return thrd;
}

PM::Thread *
PM::Process::GetThread()
{
	return LIST_FIRST(Thread, list, threads);
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
	state = S_STOPPED;
	return 0;
}

int
PM::Process::Resume()
{
	if (state != S_STOPPED) {
		return -1;
	}
	state = S_RUNNING;
	Thread *thrd;
	thrdListLock.Lock();
	LIST_FOREACH(Thread, list, thrd, threads) {
		if (thrd->state != Thread::S_TERMINATED) {
			thrd->Run();
		}
	}
	thrdListLock.Unlock();
	state = S_STOPPED;
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
	Stop();
	state = S_TERMINATED;
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
	isActive = 1;
	exitCode = 0;
	fault = PFLT_NONE;
	memset(&ctx, 0, sizeof(ctx));
	priority = DEF_PRIORITY;
	LIST_INIT(waitEntries);
	numWaitEntries = 0;
	waitString = 0;
	waitTimeout = 0;
	rqQueue = 0;
	curError = 0;
	numErrors = 0;
	gateObj = 0;
	gateCallSP = 0;
	gateCallRetAddr = 0;
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
	if (!cpu) {
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
	return 0;
}

int
PM::Thread::Terminate(int exitCode)
{
	Stop();
	state = S_TERMINATED;
	this->exitCode = exitCode;
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
		prev->isActive = 0;
		if (prev->SaveContext(&prev->ctx)) {
			/* switched to this thread */
			return;
		}
	}
	Runqueue *rq = GetRunqueue();
	rq->curThread = this;
	isActive = 1;
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
