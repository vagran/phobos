/*
 * /kernel/sys/lock.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef LOCK_H_
#define LOCK_H_
#include <sys.h>
phbSource("$Id$");

class SpinLock {
private:
	u32		flag;
public:
	SpinLock(int flag = 0);
	void Lock();
	void Unlock();
	int TryLock(); /* returns 0 if successfully locked, -1 otherwise */
};

/* Mutually exclusive access between threads */
class Mutex {
private:
	SpinLock lock;
public:
	Mutex(int flag = 0);
	void Lock();
	void Unlock();
};

class CriticalSection {
private:
	Mutex *mtx;
public:
	CriticalSection(Mutex *mtx);
	~CriticalSection();
};

#endif /* LOCK_H_ */
