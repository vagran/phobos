/*
 * /kernel/dev/sys/rtc.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <dev/sys/rtc.h>

DefineDevFactory(RTC);

RegDevClass(RTC, "rtc", Device::T_SPECIAL, "Real Time Clock");

#define	LEAPYEAR(y) ((y) % 4 == 0)
#define DAYSPERYEAR   (31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30 + 31)

const int RTC::daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

RTC::RTC(Type type, u32 unit, u32 classID) : Device(type, unit, classID)
{
	if (unit) {
		klog(KLOG_ERROR, "Only unit 0 is supported fot RTC device, specified %lu", unit);
		return;
	}
	gtc = 0;
	curTime = 0;

	if (Initialize()) {
		klog(KLOG_ERROR, "RTC hardware initializing failed");
		return;
	}
	irq = im->AllocateHwirq(IntrHandler, this, PIC::IRQ_RTC,
			IM::AF_SPEC | IM::AF_EXCLUSIVE, IM::IP_RTC);
	ensure(irq);
	im->HwEnable(PIC::IRQ_RTC);

	devState = S_UP;
}

u64
RTC::GetTime(GetTimeCbk cbk, void *arg)
{
	if (cbk) {
		/* only one callback is supported */
		u32 x = IM::DisableIntr();
		if (gtc) {
			gtc(curTime, gtcArg);
		}
		gtc = cbk;
		gtcArg = arg;
		IM::RestoreIntr(x);
	}
	return curTime;
}

int
RTC::GetRS(u32 freq)
{
	if (freq > BASE_FREQ / 4 || freq < 2) {
		return -1;
	}
	if (!ispowerof2(freq)) {
		return -1;
	}
	int rs = 3;
	while (1) {
		if ((u32)(BASE_FREQ / (1 << (rs - 1))) == freq) {
			return rs;
		}
		rs++;
	}
}

u8
RTC::Read(u8 reg)
{
	outb(ADDR_PORT, reg);
	return inb(DATA_PORT);
}

void
RTC::Write(u8 reg, u8 value)
{
	outb(ADDR_PORT, reg);
	outb(DATA_PORT, value);
}

int
RTC::BCD(u8 x)
{
	return (x >> 4) * 10 + (x & 0xf);
}

int
RTC::Initialize()
{
	u32 x = IM::DisableIntr();
	u8 d = Read(REG_STATUSA);
	Write(REG_STATUSA, (d & ~STSA_RATEMASK) | GetRS(2));
	Write(REG_STATUSB, STSB_24H | STSB_UIE | STSB_AIE);
	IM::RestoreIntr(x);
	return 0;
}

int
RTC::UpdateTime()
{
	u32 x = IM::DisableIntr();
	if (!(Read(REG_STATUSD) & STSD_VRT)) {
		curTime = 0;
		klog(KLOG_WARNING, "Invalid time in RTC (possible power failure of RTC chip)");
		IM::RestoreIntr(x);
		return -1;
	}
	int year = BCD(Read(REG_YEAR)) + BCD(Read(REG_CENTURY)) * 100;
	if (year < 1970) {
		curTime = 0;
		klog(KLOG_WARNING, "Invalid year in RTC: %d", year);
		IM::RestoreIntr(x);
		return -1;
	}
	int month = BCD(Read(REG_MONTH));
	int days = 0;
	for (int i = 1; i < month; i++) {
		days += daysInMonth[i - 1];
	}
	if (month > 2 && LEAPYEAR(year)) {
		days++;
	}
	days += BCD(Read(REG_DAY)) - 1;
	for (int i = 1970; i < year; i++) {
		days +=	DAYSPERYEAR + LEAPYEAR(i);
	}
	curTime = ((days * 24 + BCD(Read(REG_HRS))) * 60 +
		BCD(Read(REG_MIN))) * 60 + BCD(Read(REG_SEC));
	IM::RestoreIntr(x);
	return 0;
}

IM::IsrStatus
RTC::IntrHandler(Handle h, void *arg)
{
	return ((RTC *)arg)->OnIntr();
}

IM::IsrStatus
RTC::OnIntr()
{
	u32 x = IM::DisableIntr();
	u8 sts = Read(REG_STATUSC);
	if (sts & STSC_UF) {
		UpdateTime();
		if (gtc) {
			gtc(curTime, gtcArg);
			gtc = 0;
		}
	}
	IM::RestoreIntr(x);
	return IM::IS_PROCESSED;
}
