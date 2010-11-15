/*
 * /kernel/kern/tm.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

TM *tm;

TM::TM()
{
	timeValid = 0;
	syncIdx = 0;
	time = 0;
	timerIrq = 0;
	pit = (PIT *)devMan.CreateDevice("pit", 0);
	ensure(pit);
	timerFreq = pit->GetBaseFreq();
	pit->SetTickFreq(TICKS_FREQ);
	tickDivisor = pit->GetDivisor();
	ratio = (u32)(((1ull << RATIO_BITS) * 1000000ull + 500000ull) / timerFreq);
	pit->SetTickCbk(this, (PIT::TickCbk)&TM::TickHandler);
	rtc = (RTC *)devMan.CreateDevice("rtc", 0);
	if (rtc) {
		printf("Setting up system time...\n");
		SyncTime(1);
		if (!timeValid) {
			klog(KLOG_WARNING, "Failed to set system time, should be set manually");
			/* let it start from default value */
			timeValid = 1;
		}
	} else {
		klog(KLOG_WARNING, "Cannot create RTC device, system time should be set manually");
	}
	GetTime(&bootTime);
	InitTimers(&timers);
	timerIrq = im->AllocateSwirq(this, (IM::ISR)&TM::TimerIntr, 0,
		IM::AF_EXCLUSIVE, IM::IP_TIMER);
	ensure(timerIrq);
}

int
TM::SyncTime(int wait)
{
	if (!rtc) {
		return -1;
	}
	u32 idx = syncIdx;
	rtc->GetTime(SyncHandler, this);
	if (wait) {
		u32 x = IM::EnableIntr();
		while (syncIdx == idx) {
			pause();
		}
		IM::RestoreIntr(x);
	}
	return 0;
}

int
TM::SyncHandler(u64 time, void *arg)
{
	return ((TM *)arg)->SyncHandler(time);
}

int
TM::SyncHandler(u64 time)
{
	if (!time) {
		/* RTC chip failure */
		this->time = 0;
		syncCounter = 0;
		syncIdx++;
		return 0;
	}
	this->time = time;
	syncCounter = -(tickDivisor - pit->GetCounter(&syncTicks));
	syncIdx++;
	timeValid = 1;
	return 0;
}

int
TM::TickHandler(u64 ticks)
{
	if (timeValid) {
		syncCounter += (ticks - syncTicks) * tickDivisor;
		syncTicks = ticks;
		while (syncCounter >= timerFreq) {
			time++;
			syncCounter -= timerFreq;
		}
	}
	/* Schedule timer software interrupt */
	if (timerIrq) {
		im->Irq(timerIrq);
	}
	return 0;
}

u64 TM::GetTicks()
{
	u32 x = IM::DisableIntr();
	u64 ticks = syncTicks;
	IM::RestoreIntr(x);
	return ticks;
}

int
TM::GetTime(Time *t)
{
	u64 ticks;
	u32 cnt;
	do {
		ticks = GetTicks();
		t->sec = time;
		cnt = tickDivisor - pit->GetCounter();
		cnt += syncCounter;
		/* repeat if interrupt occurred */
	} while (ticks != GetTicks());
	while (cnt >= timerFreq) {
		t->sec++;
		cnt -= timerFreq;
	}
	t->usec = (cnt * ratio) >> RATIO_BITS;
	return 0;
}

int
TM::RemoveTimer(Handle h)
{
	Timer *t = (Timer *)h;
	Timers *timers = t->timers;
	u64 x = im->SetPL(IM::IP_TIMER);
	timers->lock.Lock();
	LIST_DELETE(list, t, *t->head);
	timers->lock.Unlock();
	im->RestorePL(x);
	DELETE(t);
	return 0;
}

Handle
TM::SetTimer(TimerFunc func, u64 timeToRun, void *arg, u64 period)
{
	Timer *t = NEW(Timer);
	if (!t) {
		return 0;
	}
	memset(t, 0, sizeof(*t));
	t->func = func;
	t->funcArg = arg;
	t->period = period;
	t->nextRun = timeToRun;
	t->head = 0;
	/*
	 * For the moment per CPU timers sets not supported. All timers
	 * created in default set which can be processed by any CPU.
	 */
	if (AddTimer(&timers, t)) {
		DELETE(t);
		return 0;
	}
	return t;
}

int
TM::InitTimers(Timers *timers)
{
	for (int i = 0; i < (1 << TIMER_HASHBITS_ROOT); i++) {
		LIST_INIT(timers->qRoot[i]);
	}
	for (int i = 0; i < TIMER_HASH_NODES; i++) {
		for (int j = 0; j < (1 << TIMER_HASHBITS_NODE); j++) {
			LIST_INIT(timers->qNodes[i].head[j]);
		}
	}
	timers->nextRun = GetTicks();
	return 0;
}

int
TM::AddTimer(Timers *timers, Timer *t)
{
	u64 x = im->SetPL(IM::IP_TIMER);
	timers->lock.Lock();
	t->timers = timers;
	i64 delta = t->nextRun - timers->nextRun;
	if (delta > 0xffffffffll) {
		klog(KLOG_WARNING, "Too big timeout specified, reducing to maximal allowed: %lld",
			delta);
		delta = 0xffffffffull;
		t->nextRun = timers->nextRun + delta;
	}
	ListHead *head = 0;
	if (delta < 0) {
		/* Timer expires in the past. Schedule him now. */
		head = &timers->qRoot[timers->nextRun & ((1 << TIMER_HASHBITS_ROOT) - 1)];
	} else if (delta < (1 << TIMER_HASHBITS_ROOT)) {
		head = &timers->qRoot[t->nextRun & ((1 << TIMER_HASHBITS_ROOT) - 1)];
	} else {
		for (int qn = 1; qn < TIMER_HASH_NODES; qn++) {
			if (delta < (1ll << (TIMER_HASHBITS_ROOT + qn * TIMER_HASHBITS_NODE))) {
				head = &timers->qNodes[qn - 1].head[(t->nextRun >>
					(TIMER_HASHBITS_ROOT + (qn - 1) * TIMER_HASHBITS_NODE)) &
					((1 << TIMER_HASHBITS_NODE) - 1)];
				break;
			}
		}
		if (!head) {
			head = &timers->qNodes[TIMER_HASH_NODES - 1].head[(t->nextRun >>
					(TIMER_HASHBITS_ROOT + (TIMER_HASH_NODES - 1) * TIMER_HASHBITS_NODE)) &
					((1 << TIMER_HASHBITS_NODE) - 1)];
		}
	}
	t->head = head;
	LIST_ADD(list, t, *head);
	timers->lock.Unlock();
	im->RestorePL(x);
	return 0;
}

int
TM::MigrateTimers(Timers *timers, ListHead *head)
{
	ListHead _head = *head;
	LIST_INIT(*head);
	timers->lock.Unlock();
	while (!LIST_ISEMPTY(_head)) {
		Timer *t = LIST_FIRST(Timer, list, _head);
		LIST_DELETE(list, t, _head);
		t->head = 0;
		if (AddTimer(timers, t)) {
			DELETE(t);
		}
	}
	timers->lock.Lock();
	return 0;
}

int
TM::RunTimers(Timers *timers, u64 ticks)
{
	timers->lock.Lock();
	while(timers->nextRun < ticks) {
		u32 idx = timers->nextRun & ((1 << TIMER_HASHBITS_ROOT) - 1);
		/* Migrate timers if required */
		if (!idx) {
			u32 qIdx;
			for (int qn = 0; qn < TIMER_HASH_NODES - 1; qn++) {
				qIdx = (timers->nextRun >> (TIMER_HASHBITS_ROOT + qn * TIMER_HASHBITS_NODE)) &
					((1 << TIMER_HASHBITS_NODE) - 1);
				MigrateTimers(timers, &timers->qNodes[qn].head[qIdx]);
				if (qIdx) {
					break;
				}
			}
			if (!qIdx) {
				qIdx = (timers->nextRun >> (TIMER_HASHBITS_ROOT + (TIMER_HASH_NODES - 1) * TIMER_HASHBITS_NODE)) &
					((1 << TIMER_HASHBITS_NODE) - 1);
				MigrateTimers(timers, &timers->qNodes[TIMER_HASH_NODES - 1].head[qIdx]);
			}
		}
		ListHead head = timers->qRoot[idx];
		LIST_INIT(timers->qRoot[idx]);
		timers->nextRun++;
		while (!LIST_ISEMPTY(head)) {
			Timer *t = LIST_FIRST(Timer, list, head);
			LIST_DELETE(list, t, head);
			t->head = 0;
			timers->activeTimer = t;
			timers->lock.Unlock();
			int rc = t->func(t, timers->nextRun - 1, t->funcArg);
			timers->activeTimer = 0;
			if (!rc && t->period) {
				t->nextRun += t->period;
				if (AddTimer(timers, t)) {
					DELETE(t);
				}
			} else {
				DELETE(t);
			}
			timers->lock.Lock();
		}
	}
	timers->lock.Unlock();
	return 0;
}

IM::IsrStatus
TM::TimerIntr(Handle h)
{
	RunTimers(&timers, GetTicks());
	return IM::IS_PROCESSED;
}
