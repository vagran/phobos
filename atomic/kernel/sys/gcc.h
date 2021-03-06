/*
 * /kernel/sys/gcc.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#ifndef GCC_H_
#define GCC_H_
#include <sys.h>
phbSource("$Id$");

/* pointer to class method in GCC */
typedef struct {
	enum FnFlags {
		FF_VIRTUAL =	0x1,
	};

	union {
		FUNC_PTR ptr; /* function pointer */
		u32 vOffs; /* offset in virtual methods table and flags */
	} fn;
	u32 offs;
} ClassMethodPtr;

#define VTABLE(obj)		(*(FUNC_PTR **)(obj))

class CXA {
public:
	typedef struct {
		u32		n;
		void	(*func[])();
	} CallbackList;

private:
	typedef struct {
		void	(*func)(void *);
		void	*arg;
		void	*dso_handle;
	} Destructor;


public:
	static int ConstructStaticObjects();
	static int DestructStaticObjects();
};

ASMCALL i64 __divdi3(i64 a, i64 b);
ASMCALL u64 __udivdi3(u64 a, u64 b);
ASMCALL i64	__moddi3(i64 a, i64 b);
ASMCALL u64	__umoddi3(u64 a, u64 b);
ASMCALL u64	__qdivrem(u64 u, u64 v, u64 *rem);

#endif /* GCC_H_ */
