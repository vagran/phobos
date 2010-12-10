/*
 * /phobos/lib/shlib_startup/ShlibStartup.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

extern "C" void __sl_initialize() __attribute__((section(".init")));
extern "C" void __sl_finalize() __attribute__((section(".fini")));

extern "C" CXA::CallbackList _SL_CTOR_LIST, _SL_DTOR_LIST;

extern "C" void *__dso_handle;
void *__dso_handle;

void
__sl_initialize()
{
	for (u32 i = 0; i < _SL_CTOR_LIST.n; i++) {
		_SL_CTOR_LIST.func[i]();
	}
}

void
__sl_finalize()
{
	for (u32 i = 0; i < _SL_DTOR_LIST.n; i++) {
		_SL_DTOR_LIST.func[i]();
	}
}
