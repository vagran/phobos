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
	enum {
		ADDR_PORT =		0x70,
		DATA_PORT =		0x71,

		REG_SEC =		0x00, /* Seconds */
		REG_SECALM =	0x01, /* Alarm seconds */
		REG_MIN =		0x02, /* Minutes */
		REG_MINALM =	0x03, /* Alarm minutes */
		REG_HRS =		0x04, /* Hours */
		REG_HRSALM =	0x05, /* Alarm hours */
		REG_WDAY =		0x06, /* Week day */
		REG_DAY =		0x07, /* Day of month */
		REG_MONTH =		0x08, /* Month of year */
		REG_YEAR =		0x09, /* Year */

		REG_STATUSA	=	0x0a, /* Status register A */
		STSA_UIP =		0x80, /* Update In Progress */
		STSA_DIVMASK =	0x70, /* Divider bits */
		STSA_DEFDIV =	0x20, /* Default divider for 32768Hz oscillator */
		STSA_RATEMASK =	0x0f, /* Rate Selection bits */

		REG_STATUSB =	0x0b, /* Status register B */
		STSB_DSE =		0x01, /* Daylight Saving Enable */
		STSB_24H =		0x02, /* 12/24 hours */
		STSB_DM =		0x04, /* Data Mode */
		STSB_SQWE =		0x08, /* Square Wave Enable */
		STSB_UIE =		0x10, /* Update-ended Interrupt Enabled */
		STSB_AIE =		0x20, /* Alarm Interrupt Enable */
		STSB_PIE =		0x40, /* Periodic Interrupt Enable */
		STSB_SET =		0x80, /* Set mode */

		REG_STATUSC =	0x0c, /* Status register C */
		STSC_IRQF =		0x80, /* Interrupt Request Flag */
		STSC_PF =		0x40, /* Periodic interrupt Flag */
		STSC_AF =		0x20, /* Alarm interrupt Flag */
		STSC_UF =		0x10, /* Update-ended interrupt Flag */

		REG_STATUSD =	0x0d, /* Status register D */
		STSD_VRT =		0x80, /* Valid RAM and Time */

		REG_CENTURY =	0x32, /* Century in BCD */
	};
public:
	RTC(Type type, u32 unit, u32 classID);
	DeclareDevFactory();
};

#endif /* RTC_H_ */
