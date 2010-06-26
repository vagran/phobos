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

#define Barrier() { \
	ASM ("" ::: "memory"); \
	mfence(); \
}

class AtomicOp {
public:
	static inline u32 Set(u32 *value, u32 newValue) { /* return previous value */
		u32 rc;
		ASM (
			"lock xchgl %1, %0"
			: "=m"(*value), "=r"(rc)
			: "1"(newValue)
		);
		return rc;
	}

	static inline u16 Set(u16 *value, u16 newValue) { /* return previous value */
		u16 rc;
		ASM (
			"lock xchgw %1, %0"
			: "=m"(*value), "=r"(rc)
			: "1"(newValue)
		);
		return rc;
	}

	static inline u8 Set(u8 *value, u8 newValue) { /* return previous value */
		u8 rc;
		ASM (
			"lock xchgb %1, %0"
			: "=m"(*value), "=r"(rc)
			: "1"(newValue)
		);
		return rc;
	}

	static inline void Inc(u32 *value) {
		ASM (
			"lock incl %0"
			:
			: "m"(*value)
			: "cc"
		);
	}

	static inline int Dec(u32 *value) { /* return zero if value is zero after decrement */
		int rc;
		ASM (
			"xorl	%%eax, %%eax\n"
			"lock decl	%1\n"
			"setnzb	%%al"
			: "=&a"(rc)
			: "m"(*value)
			: "cc"
		);
		return rc;
	}

	static inline u32 Add(u32 *dst, u32 src) { /* return previous value */
		u32 rc;
		ASM (
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
		ASM (
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
		ASM (
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
		ASM (
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
		ASM (
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

	static inline void Inc(u16 *value) {
		ASM (
			"lock incw %0"
			:
			: "m"(*value)
			: "cc"
		);
	}

	static inline int Dec(u16 *value) { /* return zero if value is zero after decrement */
		int rc;
		ASM (
			"xorl	%%eax, %%eax\n"
			"lock decw	%1\n"
			"setnzb	%%al"
			: "=&a"(rc)
			: "m"(*value)
			: "cc"
		);
		return rc;
	}

	static inline u16 Add(u16 *dst, u16 src) { /* return previous value */
		u16 rc;
		ASM (
			"movw	%2, %%ax\n"
			"1:\n"
			"movw	%1, %%cx\n"
			"addw	%%ax, %%cx\n"
			"lock cmpxchgw %%cx, %2\n"
			"jnz	1b\n"
			: "=&a"(rc)
			: "r"(src), "m"(*dst)
			: "cx", "cc"
		);
		return rc;
	}

	static inline u16 Sub(u16 *dst, u16 src) { /* return previous value */
		u16 rc;
		ASM (
			"movw	%2, %%ax\n"
			"1:\n"
			"movw	%%ax, %%cx\n"
			"subw	%1, %%cx\n"
			"lock cmpxchgw %%cx, %2\n"
			"jnz	1b\n"
			: "=&a"(rc)
			: "r"(src), "m"(*dst)
			: "cx", "cc"
		);
		return rc;
	}

	static inline u16 And(u16 *dst, u16 src) { /* return previous value */
		u16 rc;
		ASM (
			"movw	%2, %%ax\n"
			"1:\n"
			"movw	%1, %%cx\n"
			"andw	%%ax, %%cx\n"
			"lock cmpxchgw %%cx, %2\n"
			"jnz	1b\n"
			: "=&a"(rc)
			: "r"(src), "m"(*dst)
			: "cx", "cc"
		);
		return rc;
	}

	static inline u16 Or(u16 *dst, u16 src) { /* return previous value */
		u16 rc;
		ASM (
			"movw	%2, %%ax\n"
			"1:\n"
			"movw	%1, %%cx\n"
			"orw	%%ax, %%cx\n"
			"lock cmpxchgw %%cx, %2\n"
			"jnz	1b\n"
			: "=&a"(rc)
			: "r"(src), "m"(*dst)
			: "cx", "cc"
		);
		return rc;
	}

	static inline u16 Xor(u16 *dst, u16 src) { /* return previous value */
		u16 rc;
		ASM (
			"movw	%2, %%ax\n"
			"1:\n"
			"movw	%1, %%cx\n"
			"xorw	%%ax, %%cx\n"
			"lock cmpxchgw %%cx, %2\n"
			"jnz	1b\n"
			: "=&a"(rc)
			: "r"(src), "m"(*dst)
			: "cx", "cc"
		);
		return rc;
	}

	static inline void Inc(u8 *value) {
		ASM (
			"lock incb %0"
			:
			: "m"(*value)
			: "cc"
		);
	}

	static inline int Dec(u8 *value) { /* return zero if value is zero after decrement */
		int rc;
		ASM (
			"xorl	%%eax, %%eax\n"
			"lock decb	%1\n"
			"setnzb	%%al"
			: "=&a"(rc)
			: "m"(*value)
			: "cc"
		);
		return rc;
	}
};

#include <object.h>

class SpinLock : public Object {
private:
	u32		flag;
public:
	SpinLock(int flag = 0);
	~SpinLock() { assert(!flag); }
	void Lock();
	void Unlock();
	int TryLock(); /* returns 0 if successfully locked, -1 otherwise */
};

template <typename value_t>
class AtomicInt : public Object {
private:
	value_t value;
public:
	inline AtomicInt() { value = 0; }
	inline value_t operator =(value_t x) { AtomicOp::Set(&value, x); return x; }
	inline void operator ++() { AtomicOp::Inc(&value); }
	inline int operator --() { return AtomicOp::Dec(&value); }
	inline operator value_t() { return value; }
};

/* Mutually exclusive access between processors, recursion allowed */
class CPUMutex : public Object {
private:
	SpinLock lock;
	void *cpu;
	u32 count;
public:
	CPUMutex(int flag = 0);
	void Lock();
	void Unlock();
	int TryLock(); /* returns 0 if successfully locked, -1 otherwise */
};

/* Mutually exclusive access between threads */
class Mutex : public Object {
private:
	SpinLock lock;
public:
	Mutex(int flag = 0);
	void Lock();
	void Unlock();
	int TryLock(); /* returns 0 if successfully locked, -1 otherwise */
};

class CriticalSection : public Object {
private:
	Mutex *mtx;
public:
	CriticalSection(Mutex *mtx);
	~CriticalSection();
};

#endif /* LOCK_H_ */
