/*
 * /kernel/dev/sys/pit.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef PIT_H_
#define PIT_H_
#include <sys.h>
phbSource("$Id$");

/* 8253/8254 Programmable Interval Timer support */

class PIT : public Device {
private:
	enum {
		IO_BASE =		0x40, /* Base address for I/O */

		PORT_CNT0 =		IO_BASE + 0,
		PORT_CNT1 =		IO_BASE + 1,
		PORT_CNT2 =		IO_BASE + 2,
		PORT_MODE =		IO_BASE + 3,

		SEL_TMR0 =		0x00,
		SEL_TMR1 =		0x40,
		SEL_TMR2 =		0x80,

		MODE_INTTC =	0x00, /* mode 0, interrupt on terminal count */
		MODE_ONESHOT =	0x02, /* mode 1, one shot */
		MODE_RATEGEN =	0x04, /* mode 2, rate generator */
		MODE_SQWAVE =	0x06, /* mode 3, square wave */
		MODE_SWSTROBE =	0x08, /* mode 4, software triggered strobe */
		MODE_HWSTROBE =	0xa0, /* mode 5, h/w triggered strobe */

		MODE_LSB =		0x10,
		MODE_MSB =		0x20,
		MODE_16BIT =	0x30,

		MODE_BCD =		0x01,
	};

public:
	PIT(Type type, u32 unit, u32 classID);
	DeclareDevFactory();
};

#endif /* PIT_H_ */
