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
#endif /* !defined(KERNEL) && !defined(UNIT_TEST) */

#ifndef KERNEL

class ULib : public Object {
private:
	GApp *app;
	USlabAllocator alloc;
public:
	ULib(GApp *app);
	~ULib();
	static ULib *Initialize(GApp *app);

	inline GApp *GetApp() { return app; }
	inline MemAllocator *GetMemAllocator() { return &alloc; }
};

extern ULib *uLib;

#endif /* KERNEL */

#endif /* USER_H_ */
