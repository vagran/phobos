/*
 * /kernel/dev/sys/pit.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <dev/sys/pit.h>

DefineDevFactory(PIT);

RegDevClass(PIT, "pit", Device::T_SPECIAL, "Programmable Interval Timer (8253/8254)");

PIT::PIT(Type type, u32 unit, u32 classID) : Device(type, unit, classID)
{
	if (unit) {
		klog(KLOG_ERROR, "Only unit 0 is supported fot PIT device, specified %lu", unit);
		return;
	}
	tickFreq = 0;
	ticks = 0;
	tickCbk = 0;
	SetTickFreq(DEF_TICK_FREQ);
	irq = im->AllocateHwirq(IntrHandler, this, PIC::IRQ_PIT,
		IM::AF_SPEC | IM::AF_EXCLUSIVE, IM::IP_CLOCK);
	ensure(irq);
	im->HwEnable(PIC::IRQ_PIT);

	devState = S_UP;
}

u32
PIT::SetTickFreq(u32 freq)
{
	if (!freq || freq > BASE_FREQ) {
		return 0;
	}
	if (tickFreq == freq) {
		return divisor;
	}
	divisor = (BASE_FREQ + freq / 2) / freq;
	if (divisor > 0xffff) {
		klog(KLOG_WARNING, "Cannot set frequency %lu for PIT, divisor out of range", freq);
		return 0;
	}
	tickFreq = freq;
	realTickFreq = BASE_FREQ / divisor;
	accLock.Lock();
	u32 x = IM::DisableIntr();
	outb(PORT_CTRL, SEL_TMR0 | MODE_RATEGEN | CTRL_16BIT);
	outb(PORT_CNT0, divisor & 0xff);
	outb(PORT_CNT0, divisor >> 8);
	IM::RestoreIntr(x);
	accLock.Unlock();
	return divisor;
}

u32
PIT::GetCounter()
{
	accLock.Lock();
	u32 x = IM::DisableIntr();
	outb(PORT_CTRL, SEL_TMR0);
	u8 lo = inb(PORT_CNT0);
	u8 hi = inb(PORT_CNT0);
	IM::RestoreIntr(x);
	accLock.Unlock();
	return lo | (hi << 8);
}

IM::IsrStatus
PIT::IntrHandler(HANDLE h, void *arg)
{
	return ((PIT *)arg)->OnIntr();
}

PIT::TickCbk
PIT::SetTickCbk(TickCbk cbk, void *arg, void **prevArg)
{
	TickCbk prev = tickCbk;
	if (prevArg) {
		*prevArg = tickCbkArg;
	}
	tickCbk = cbk;
	tickCbkArg = arg;
	return prev;
}

IM::IsrStatus
PIT::OnIntr()
{
	ticks++;
	if (tickCbk) {
		tickCbk(ticks, tickCbkArg);
	}
	return IM::IS_PROCESSED;
}
