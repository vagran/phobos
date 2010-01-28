/*
 * /kernel/dev/sys/rtc.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef RTC_H_
#define RTC_H_
#include <sys.h>
phbSource("$Id$");

/* Real Time Clock */

class RTC : public Device {
private:

public:
	RTC(Type type, u32 unit, u32 classID);
	DeclareDevFactory();
};

#endif /* RTC_H_ */
