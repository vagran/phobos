/*
 * /kernel/kern/lock.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

/*********************************************/
/* SpinLock */

SpinLock::SpinLock(int flag)
{
	this->flag = flag;
}

void
SpinLock::Lock()
{
	ASM (
		"1: lock btsl	$0, %0\n"
		"jnc	2f\n"
		"pause\n"
		"jmp	1b\n"
		"2:\n"
		:
		: "m"(flag)
		: "cc"
		);
}

void
SpinLock::Unlock()
{
	ASM (
		"lock btcl	$0, %0"
		:
		: "m"(flag)
		: "cc"
		);
}

int
SpinLock::TryLock()
{
	register int rc;
	ASM (
		"xorl	%%eax, %%eax\n"
		"lock btsl	$0, %1\n"
		"jnc	1f\n"
		"movl	$-1, %%eax\n"
		"1:\n"
		: "=&a"(rc)
		: "m"(flag)
		: "cc"
		);
	return rc;
}

#ifdef KERNEL

/*********************************************/
/* CPUMutex */

CPUMutex::CPUMutex(int flag) : lock(flag)
{
	if (flag) {
		cpu = CPU::GetCurrent();
		count = 1;
	} else {
		cpu = 0;
		count = 0;
	}
}

void
CPUMutex::Lock()
{
	void *curCPU = CPU::GetCurrent();
	if (lock.TryLock()) {
		if (curCPU && cpu == curCPU) {
			count++;
			return;
		}
		lock.Lock();
	}
	assert(!count);
	cpu = curCPU;
	count++;
}

void
CPUMutex::Unlock()
{
	assert((u32)count);
	assert(!cpu || CPU::GetCurrent() == cpu);
	--count;
	if (!count) {
		cpu = 0;
		lock.Unlock();
	}
}

int
CPUMutex::TryLock()
{
	void *curCPU = CPU::GetCurrent();
	if (lock.TryLock()) {
			if (curCPU && cpu == curCPU) {
				count++;
				return 0;
			}
			return -1;
	}
	assert(!count);
	cpu = curCPU;
	count++;
	return 0;
}

/*********************************************/
/* Mutex */

Mutex::Mutex(int flag) : lock(flag)
{
	useSleep = pm && PM::Thread::GetCurrent();
}

void
Mutex::Lock()
{
	if (useSleep) {
		pm->ReserveSleepChannel(this);
	}
	while (lock.TryLock()) {
		if (useSleep) {
			pm->Sleep(this, "Mutex::Lock");
		} else {
			pause();
		}
	}
}

int
Mutex::TryLock()
{
	return lock.TryLock();
}

void
Mutex::Unlock()
{
	lock.Unlock();
	if (useSleep) {
		pm->Wakeup(this);
	}
}

/*********************************************/
/* CriticalSection */

CriticalSection::CriticalSection(Mutex *mtx)
{
	assert(mtx);
	this->mtx = mtx;
	mtx->Lock();
}

CriticalSection::~CriticalSection()
{
	mtx->Unlock();
}

#endif /* KERNEL */
