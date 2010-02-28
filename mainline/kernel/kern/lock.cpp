/*
 * /kernel/kern/lock.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
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
	__asm__ __volatile__ (
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
	__asm__ __volatile__ (
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
	__asm__ __volatile__ (
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

/*********************************************/
/* Mutex */

Mutex::Mutex(int flag) : lock(flag)
{

}

void
Mutex::Lock()
{
	if (pm) {
		pm->ReserveSleepChannel(this);
	}
	while (lock.TryLock()) {
		if (pm) {
			pm->Sleep(this, "Mutex::Lock");
		}
	}
}

void
Mutex::Unlock()
{
	lock.Unlock();
	if (pm) {
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
