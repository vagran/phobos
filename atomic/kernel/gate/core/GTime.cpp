/*
 * /phobos/kernel/gate/GTime.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#include <sys.h>
phbSource("$Id$");

DEFINE_GCLASS(GTime);

GTime::GTime()
{

}

GTime::~GTime()
{

}

u32
GTime::MS(u64 ms, u64 *pTicks)
{
	u64 ticks = tm->MS(ms);
	if (pTicks) {
		if (proc->CheckUserBuf(pTicks, sizeof(*pTicks), MM::PROT_WRITE)) {
			return 0;
		}
		*pTicks = ticks;
	}
	return (u32)ticks;
}

u32
GTime::S(u64 s, u64 *pTicks)
{
	u64 ticks = tm->S(s);
	if (pTicks) {
		if (proc->CheckUserBuf(pTicks, sizeof(*pTicks), MM::PROT_WRITE)) {
			return 0;
		}
		*pTicks = ticks;
	}
	return (u32)ticks;
}
