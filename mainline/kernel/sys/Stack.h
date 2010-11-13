/*
 * /phobos/kernel/sys/Stack.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#ifndef STACK_H_
#define STACK_H_
#include <sys.h>
phbSource("$Id$");

template <class Allocator, typename T, int STATIC_SIZE = 32>
class Stack : public Object {
private:
	enum {
		ALLOC_GRAN =		32,
	};

	T staticStorage[STATIC_SIZE];
	T *storage;
	int depth, size;
	Allocator alloc;

	inline void *malloc(u32 size) { return alloc.malloc(size); }
	inline void *mrealloc(void *p, u32 size) { return alloc.mrealloc(p, size); }
	inline void mfree(void *p) { alloc.mfree(p); }
public:
	inline Stack() {
		depth = 0;
		storage = staticStorage;
		size = STATIC_SIZE;
	}

	inline ~Stack() {
		if (storage != staticStorage) {
			mfree(storage);
		}
	}

	inline int GetDepth() { return depth; }

	inline int Push(T el) {
		if (depth >= size) {
			if (storage != staticStorage) {
				storage = (T *)mrealloc(storage, (size + ALLOC_GRAN) * sizeof(T));
			} else {
				storage = (T *)malloc((size + ALLOC_GRAN) * sizeof(T));
			}
		}
		storage[depth++] = el;
		return depth;
	}

	inline T Pop() {
		assert(depth);
		depth--;
		return storage[depth];
	}
};

template <typename T, int STATIC_SIZE = 32> class KStack :
	public Stack<KMemAllocator, T, STATIC_SIZE> {};

#ifdef KERNEL
#define STACK KStack
#else /* KERNEL */
#ifndef UNIT_TEST
#define STACK CStack
#endif /* UNIT_TEST */
#endif /* KERNEL */

#endif /* STACK_H_ */
