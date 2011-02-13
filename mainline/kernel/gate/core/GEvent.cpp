/*
 * /phobos/kernel/gate/core/GEvent.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
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

PM::waitid_t
GEvent::GetWaitChannel(Operation op)
{
	if (!ev || (op != OP_READ && op != OP_WRITE)) {
		return 0;
	}
	return (PM::waitid_t)ev;
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
