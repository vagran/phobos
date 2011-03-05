/*
 * /phobos/kernel/gate/core/GEvent.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#ifndef GEVENT_H_
#define GEVENT_H_
#include <sys.h>
phbSource("$Id$");

DECLARE_GCLASS(GEvent);

class GEvent : public GateObject {
private:
	typedef struct {
		SpinLock lock;
		u32 state; /* 0/1 - Reset/Set */
	} Event;

	Event *ev;
public:
	GEvent();
	virtual ~GEvent();

	virtual waitid_t GetWaitChannel(Operation op);
	virtual int OpAvailable(Operation op);

	virtual int Set();
	virtual int Reset();
	virtual int Pulse();

	DECLARE_GCLASS_IMP(GEvent);
};

#endif /* GEVENT_H_ */
