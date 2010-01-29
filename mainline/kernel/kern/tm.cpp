/*
 * /kernel/kern/tm.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

TM *tm;

TM::TM()
{
	timeValid = 0;
	syncIdx = 0;
	time = 0;
	pit = (PIT *)devMan.CreateDevice("pit", 0);
	ensure(pit);
	timerFreq = pit->GetBaseFreq();
	tickDivisor = pit->GetDivisor();
	ratio = (u32)(((1ull << RATIO_BITS) * 1000000ull + 500000ull) / timerFreq);
	pit->SetTickCbk(TickHandler, this);
	rtc = (RTC *)devMan.CreateDevice("rtc", 0);
	if (rtc) {
		printf("Setting up system time...\n");
		SyncTime(1);
	} else {
		klog(KLOG_WARNING, "Cannot create RTC device, system time should be set manually");
	}
	GetTime(&bootTime);
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
	this->time = time;
	syncCounter = tickDivisor - pit->GetCounter();
	syncTicks = pit->GetTicks();
	syncIdx++;
	timeValid = 1;
	return 0;
}

int
TM::TickHandler(u64 ticks, void *arg)
{
	return ((TM *)arg)->TickHandler(ticks);
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
	return 0;
}

int
TM::GetTime(Time *t)
{
	u64 ticks;
	u32 cnt;
	do {
		ticks = syncTicks;
		t->sec = time;
		cnt = tickDivisor - pit->GetCounter();
		cnt += syncCounter;
		/* repeat if interrupt occurred */
	} while (ticks != syncTicks);
	while (cnt >= timerFreq) {
		t->sec++;
		cnt -= timerFreq;
	}
	t->usec = (cnt * ratio) >> RATIO_BITS;
	return 0;
}
