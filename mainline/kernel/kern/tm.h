/*
 * /kernel/kern/tm.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef TM_H_
#define TM_H_
#include <sys.h>
phbSource("$Id$");

#include <dev/sys/pit.h>
#include <dev/sys/rtc.h>

/* Time Manager */

typedef struct {
	u64 sec;
	u32 usec;
} Time;

class TM {
public:

private:
	enum {
		RATIO_BITS =	11,
	};

	PIT *pit;
	RTC *rtc;

	u64 time; /* current system time */
	int timeValid;
	u32 tickDivisor;
	u32 timerFreq;
	u32 syncIdx;
	u64 syncTicks;
	u32 syncCounter;
	u32 ratio;
	Time bootTime;

	static int TickHandler(u64 ticks, void *arg);
	int TickHandler(u64 ticks);
	static int SyncHandler(u64 time, void *arg);
	int SyncHandler(u64 time);
	int SyncTime(int wait = 0);
public:
	TM();

	int GetTime(Time *t);
};

extern TM *tm;

#endif /* TM_H_ */
