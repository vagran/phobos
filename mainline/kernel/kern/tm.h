/*
 * /kernel/kern/tm.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
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

class TM : public Object {
public:
	enum {
		TICKS_FREQ =			1000,
	};

	typedef int (*TimerFunc)(Handle h, u64 ticks, void *arg); /* return non-zero to delete periodic timer */

private:
	enum {
		RATIO_BITS =			11,
		TIMER_HASHBITS_ROOT =	8,
		TIMER_HASHBITS_NODE =	6,
		TIMER_HASH_NODES =		4,
	};

	struct _Timer;
	typedef struct _Timer Timer;

	typedef struct {
		u64 nextRun; /* in ticks */
		ListHead qRoot[1 << TIMER_HASHBITS_ROOT];
		struct {
			ListHead head[1 << TIMER_HASHBITS_NODE];
		} qNodes[TIMER_HASH_NODES];
		SpinLock lock;
		Timer *activeTimer;
	} Timers;

	struct _Timer {
		ListEntry list;
		u64 nextRun;
		u64 period; /* one shot if zero */
		TimerFunc func;
		void *funcArg;
		ListHead *head; /* in which list we are */
		Timers *timers;
	};

	PIT *pit;
	RTC *rtc;

	u64 time; /* current system time */
	int timeValid; /* time valid after synchronizing with RTC */
	u32 tickDivisor;
	u32 timerFreq;
	u32 syncIdx;
	u64 syncTicks; /* current ticks counter */
	u32 syncCounter;
	u32 ratio; /* integer value to convert timer ticks to microseconds */
	Time bootTime;
	Timers timers; /* default timers set */
	Handle timerIrq;

	int TickHandler(u64 ticks);
	static int SyncHandler(u64 time, void *arg);
	int SyncHandler(u64 time);
	int SyncTime(int wait = 0);
	int AddTimer(Timers *timers, Timer *t);
	int InitTimers(Timers *timers);
	int MigrateTimers(Timers *timers, ListHead *head);
	int RunTimers(Timers *timers, u64 ticks);
	IM::IsrStatus TimerIntr(Handle h);
public:
	TM();

	int GetTime(Time *t);
	u64 GetTicks();
	inline u64 MS(u64 ms) {return ms * timerFreq / 1000 / tickDivisor;}
	inline u64 S(u64 s) {return s * timerFreq / tickDivisor;}
	Handle SetTimer(TimerFunc func, u64 timeToRun, void *arg = 0, u64 period = 0);
	int RemoveTimer(Handle h);
};

extern TM *tm;

#endif /* TM_H_ */
