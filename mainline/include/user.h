/*
 * /phobos/include/user.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

/*
 * User space runtime support library.
 * This file is included by every user space component.
 */

#ifndef USER_H_
#define USER_H_
#include <sys.h>
phbSource("$Id$");

#include <gate/gate.h>
#include <../lib/user/mem.h>

#if !defined(KERNEL) && !defined(UNIT_TEST)
typedef class String<UMemAllocator> CString;
template <typename T, int STATIC_SIZE = 32> class CStack :
	public Stack<UMemAllocator, T, STATIC_SIZE> {};
#endif /* !defined(KERNEL) && !defined(UNIT_TEST) */

#ifndef KERNEL

class ULib : public Object {
private:
	GApp *app;
	GProcess *proc;
	GVFS *vfs;
	GStream *sOut, *sIn, *sTrace, *sLog;
	USlabAllocator alloc;
public:
	ULib(GApp *app);
	~ULib();
	static ULib *Initialize(GApp *app);

	inline GApp *GetApp() { return app; }
	inline GProcess *GetProcess() { return proc; }
	inline GVFS *GetVFS() { return vfs; }
	inline MemAllocator *GetMemAllocator() { return &alloc; }

	inline GStream *GetOut() { return sOut; }
	inline GStream *GetIn() { return sIn; }
	inline GStream *GetTrace() { return sTrace; }
	inline GStream *GetLog() { return sLog; }

	inline void *malloc(u32 size) { return alloc.malloc(size); }
	inline void *mrealloc(void *p, u32 size) { return alloc.mrealloc(p, size); }
	inline void mfree(void *p) { alloc.mfree(p); }

	int StreamPrintfV(GStream *s, const char *fmt, va_list args) __format(printf, 3, 0);
	int StreamPrintf(GStream *s, const char *fmt,...) __format(printf, 3, 4);
};

extern ULib *uLib;

static inline void vprintf(const char *fmt, va_list va) __format(printf, 1, 0);
static inline void vprintf(const char *fmt, va_list va) {
	uLib->StreamPrintfV(uLib->GetOut(), fmt, va);
}

static inline void printf(const char *fmt,...)	__format(printf, 1, 2);
static inline void printf(const char *fmt,...) {
	va_list va;
	va_start(va, fmt);
	vprintf(fmt, va);
}

static inline void *malloc(u32 size) {
	return uLib->malloc(size);
}

static inline void *mrealloc(void *p, u32 size) {
	return uLib->mrealloc(p, size);
}

static inline void mfree(void *p) {
	return uLib->mfree(p);
}

#endif /* KERNEL */

#endif /* USER_H_ */
