/*
 * /phobos/bin/module_testing/dynamic/multithreading.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <module_test.h>

class MTMultiThreading : public MTest {
private:
public:
	MT_DECLARE(MTMultiThreading);
};

MT_DESTR(MTMultiThreading) {}

static int
ThreadEntry(void *arg)
{
	mt_assert((u32)arg == MT_DWORD_VALUE);
	return MT_DWORD_VALUE2;
}

GEvent *ev1, *ev2;
int thread2Started;
static int
ThreadEntry2(void *arg)
{
	thread2Started = 1;
	mt_assert((u32)arg == MT_DWORD_VALUE);
	GApp *app = uLib->GetApp();
	mt_assert(app->Wait(GEvent::OP_READ, ev1) == 1);
	ev2->Pulse();
	return MT_DWORD_VALUE2;
}


#define NUM_THREADS	1

static int threadStarted[NUM_THREADS];
static int finishThreads;
static int
ThreadEntry3(void *arg)
{
	GApp *app = uLib->GetApp();
	GThread *thrd = app->GetThread();
	PM::pid_t pid = thrd->GetPID();
	printf("Thread %lu (pid = %d) started\n", (u32)arg, pid);
	threadStarted[(int)arg] = 1;
	while (!finishThreads);
	printf("Thread %lu (pid = %d) exiting\n", (u32)arg, pid);
	return MT_DWORD_VALUE2 + (int)arg;
}

MT_DEFINE(MTMultiThreading, "Multi-threading verification")
{
	GApp *app = uLib->GetApp();
	GProcess *proc = app->GetProcess();
	printf("MTMultiThreading pid = %d\n", app->GetThread()->GetPID());//temp
	GThread *thrd = uLib->CreateThread(proc, ThreadEntry, (void *)MT_DWORD_VALUE);
	mt_assert(thrd);
	app->Wait(GThread::OP_READ, thrd);
	u32 exitCode = 0;
	PM::ProcessFault fault = PM::PFLT_ABORT;
	mt_assert(thrd->GetState(&exitCode, &fault) == PM::Thread::S_TERMINATED);
	mt_assert(fault == PM::PFLT_NONE);
	mt_assert(exitCode == MT_DWORD_VALUE2);
	thrd->Release();

	/* Test events */
	exitCode = 0;
	fault = PM::PFLT_ABORT;
	mt_assert((ev1 = app->CreateEvent()));
	mt_assert((ev2 = app->CreateEvent()));
	thrd = uLib->CreateThread(proc, ThreadEntry2, (void *)MT_DWORD_VALUE);
	mt_assert(thrd);
	int loopCount = 0;
	while (!thread2Started) {
		app->Sleep(uLib->GetTime()->S(1));
		mt_assert(loopCount < 5);
		loopCount++;
	}
	ev1->Pulse();
	mt_assert(app->Wait(GEvent::OP_READ, ev2) == 1);
	mt_assert(thrd->GetState(&exitCode, &fault) == PM::Thread::S_TERMINATED);
	mt_assert(fault == PM::PFLT_NONE);
	mt_assert(exitCode == MT_DWORD_VALUE2);
	thrd->Release();

	/* Test concurrent threads */
	GThread *threads[NUM_THREADS];
	for (int i = 0; i < NUM_THREADS; i++) {
		threads[i] = uLib->CreateThread(proc, ThreadEntry3, (void *)i);
		mt_assert(threads[i]);
	}
	app->Sleep(uLib->GetTime()->S(1));
	for (int i = 0; i < NUM_THREADS; i++) {
		mt_assert(threadStarted[i]);
	}
	finishThreads = 1;
	app->Sleep(uLib->GetTime()->S(10000));
	for (int i = 0; i < NUM_THREADS; i++) {
		mt_assert(threads[i]->GetState(&exitCode, &fault) ==
			PM::Thread::S_TERMINATED);
		mt_assert(fault == PM::PFLT_NONE);
		mt_assert(exitCode == MT_DWORD_VALUE2 + i);
		threads[i]->Release();
	}

	proc->Release();
}
