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

class AtomicOp {
public:
	static inline void Increment(u32 *value) {
		__asm__ __volatile__ (
			"lock incl %0"
			:
			: "m"(*value)
			: "cc"
		);
	}

	static inline int Decrement(u32 *value) { /* return zero if value is zero after decrement */
		int rc;
		__asm__ __volatile__ (
			"xorl	%%eax, %%eax\n"
			"lock decl	%1\n"
			"setnz	%%al"
			: "=&a"(rc)
			: "m"(*value)
			: "cc"
		);
		return rc;
	}

	static inline u32 Add(u32 *dst, u32 src) { /* return previous value */
		u32 rc;
		__asm__ __volatile__ (
			"movl	%2, %%eax\n"
			"1:\n"
			"movl	%1, %%ecx\n"
			"addl	%%eax, %%ecx\n"
			"lock cmpxchgl %%ecx, %2\n"
			"jnz	1b\n"
			: "=&a"(rc)
			: "r"(src), "m"(*dst)
			: "ecx", "cc"
		);
		return rc;
	}

	static inline u32 Sub(u32 *dst, u32 src) { /* return previous value */
		u32 rc;
		__asm__ __volatile__ (
			"movl	%2, %%eax\n"
			"1:\n"
			"movl	%%eax, %%ecx\n"
			"subl	%1, %%ecx\n"
			"lock cmpxchgl %%ecx, %2\n"
			"jnz	1b\n"
			: "=&a"(rc)
			: "r"(src), "m"(*dst)
			: "ecx", "cc"
		);
		return rc;
	}

	static inline u32 And(u32 *dst, u32 src) { /* return previous value */
		u32 rc;
		__asm__ __volatile__ (
			"movl	%2, %%eax\n"
			"1:\n"
			"movl	%1, %%ecx\n"
			"andl	%%eax, %%ecx\n"
			"lock cmpxchgl %%ecx, %2\n"
			"jnz	1b\n"
			: "=&a"(rc)
			: "r"(src), "m"(*dst)
			: "ecx", "cc"
		);
		return rc;
	}

	static inline u32 Or(u32 *dst, u32 src) { /* return previous value */
		u32 rc;
		__asm__ __volatile__ (
			"movl	%2, %%eax\n"
			"1:\n"
			"movl	%1, %%ecx\n"
			"orl	%%eax, %%ecx\n"
			"lock cmpxchgl %%ecx, %2\n"
			"jnz	1b\n"
			: "=&a"(rc)
			: "r"(src), "m"(*dst)
			: "ecx", "cc"
		);
		return rc;
	}

	static inline u32 Xor(u32 *dst, u32 src) { /* return previous value */
		u32 rc;
		__asm__ __volatile__ (
			"movl	%2, %%eax\n"
			"1:\n"
			"movl	%1, %%ecx\n"
			"xorl	%%eax, %%ecx\n"
			"lock cmpxchgl %%ecx, %2\n"
			"jnz	1b\n"
			: "=&a"(rc)
			: "r"(src), "m"(*dst)
			: "ecx", "cc"
		);
		return rc;
	}
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
