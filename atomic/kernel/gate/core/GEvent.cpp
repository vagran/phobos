/*
 * /phobos/kernel/gate/core/GEvent.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#include <sys.h>
phbSource("$Id$");

DEFINE_GCLASS(GEvent);

GEvent::GEvent()
{
	if (!(ev = NEW(Event))) {
		ERROR(E_NOMEM, "Cannot allocate event object");
	}
}

GEvent::~GEvent()
{
	if (ev) {
		DELETE(ev);
	}
}

int
GEvent::OpAvailable(Operation op)
{
	if (!ev || (op != OP_READ && op != OP_WRITE)) {
		return 0;
	}
	ev->lock.Lock();
	u32 state = ev->state;
	ev->lock.Unlock();
	return state;
}

waitid_t
GEvent::GetWaitChannel(Operation op)
{
	if (!ev || (op != OP_READ && op != OP_WRITE)) {
		return 0;
	}
	return (waitid_t)ev;
}

int
GEvent::Set()
{
	ev->lock.Lock();
	int prevState = ev->state;
	ev->state = 1;
	ev->lock.Unlock();
	pm->Wakeup(ev);
	return prevState;
}

int
GEvent::Reset()
{
	ev->lock.Lock();
	int prevState = ev->state;
	ev->state = 0;
	ev->lock.Unlock();
	return prevState;
}

int
GEvent::Pulse()
{
	ev->lock.Lock();
	int prevState = ev->state;
	ev->state = 0;
	ev->lock.Unlock();
	pm->Wakeup(ev);
	return prevState;
}
