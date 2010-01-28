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

RTC::RTC(Type type, u32 unit, u32 classID) : Device(type, unit, classID)
{
	if (unit) {
		klog(KLOG_ERROR, "Only unit 0 is supported fot RTC device, specified %lu", unit);
		return;
	}

	devState = S_UP;
}
