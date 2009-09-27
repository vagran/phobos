/*
 * /kernel/lib/gcc.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <gcc.h>

extern "C" int __dso_handle;

extern "C" CXA::CallbackList _CTOR_LIST, _DTOR_LIST;

ASMCALL int
__cxa_atexit(void (*func)(void *), void * arg, void *dso_handle)
{

	return 0;
}

int
CXA::ConstructStaticObjects()
{
	for (u32 i = 0; i < _CTOR_LIST.n; i++) {
		_CTOR_LIST.func[i]();
	}
	return 0;
}

int
CXA::DestructStaticObjects()
{
	for (u32 i = 0; i < _DTOR_LIST.n; i++) {
		_DTOR_LIST.func[i]();
	}
	return 0;
}
