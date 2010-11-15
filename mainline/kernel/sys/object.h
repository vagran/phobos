/*
 * /phobos/kernel/sys/object.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#ifndef OBJECT_H_
#define OBJECT_H_
#include <sys.h>
phbSource("$Id$");

class RefCount {
private:
	u32 count;
public:
	inline RefCount() { count = 1; }
	inline ~RefCount() { assert(!count); }

	inline void Inc() { AtomicOp::Inc(&count); }
	inline int Dec() { assert(count); return AtomicOp::Dec(&count); }
	inline operator u32() { return count; }
	inline void operator ++() { Inc(); }
	inline int operator --() { return Dec(); }
};

/* standard methods declarations for kernel objects */
#define OBJ_ADDREF(refCount)	inline void AddRef() { \
	refCount.Inc(); \
}

/* return zero if object was actually freed */
#define OBJ_RELEASE(refCount)		inline int Release() { \
	int rc = refCount.Dec(); \
	if (!rc) { \
		DELETE(this); \
	} \
	return rc; \
}

/* signature to guard against memory corruption */
#define OBJ_GUARDSIGN		0x12345678

/*
 * Most of the kernel classes are derived from this class. It allows at least
 * usage of convenient callback methods notation and advanced debugging
 * features.
 */
class Object {
private:
	static u32 objCount;
#ifdef DEBUG
	u32 guardSign;
#endif /* DEBUG */
protected:

public:
	inline Object() {
		AtomicOp::Inc(&objCount);
#ifdef DEBUG
		guardSign = OBJ_GUARDSIGN;
#endif
	}
	inline ~Object() {
		AtomicOp::Dec(&objCount);
		assert(guardSign == OBJ_GUARDSIGN);
	}

	inline u32 GetCount() { return objCount; }
};

#endif /* OBJECT_H_ */
