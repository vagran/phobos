/*
 * /phobos/kernel/dev/chr/KbdController.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#ifndef KBDCONTROLLER_H_
#define KBDCONTROLLER_H_
#include <sys.h>
phbSource("$Id$");

#include <dev/chr/KbdDev.h>

/* Implementation of old 8042 keyboard controller */

class KbdController : public KbdDev	{
private:
	enum {
		CODE_ESC =				0xe0,
		CODE_ESC1 =				0xe1,
		CODE_RELEASED =			0x80,
		CODE_MASK =				0x7f,
	};

	enum Registers {
		REG_STATUS =			0x64, /* Status register */
		REG_STATUS_OBF =		0x1, /* Output Buffer Full */
		REG_STATUS_IBF =		0x2, /* Input Buffer Full */
		REG_STATUS_SF =			0x4, /* System Flag */
		REG_STATUS_CD =			0x8, /* Command/Data */
		REG_STATUS_IS =			0x10, /* Inhibit Switch */
		REG_STATUS_TTO =		0x20, /* Transmit Time-Out */
		REG_STATUS_RTO =		0x40, /* Receive Time-Out */
		REG_STATUS_PE =			0x80, /* Parity Error */

		REG_INPUT =				0x60, /* Input register */

		REG_OUTPUT =			0x60, /* Output register */

		REG_COMMAND =			0x64, /* Command register */

		/* Command bytes bits */
		CMDB_IBM_COMP =			0x40, /* IBM PC compatibility */
		CMDB_IBM_MODE =			0x20, /* IBM PC mode */
		CMDB_DISABLE_KBD =		0x10, /* Disable keyboard */
		CMDB_INHB_OVERRIDE =	0x08, /* Inhibit override */
		CMDB_SF =				0x04, /* System Flag */
		CMDB_OBFI =				0x01, /* Enable Output Buffer Full interrupt */
	};

	enum Commands {
		ACK =					0xfa, /* Acknowledge from controller */
		ECHO =					0xee, /* Echo response */

		CMD_NONE =				-1,   /* For Command() method */
		CMD_READ_COMMAND =		0x20, /* Read command byte */
		CMD_WRITE_COMMAND =		0x60, /* Write command byte */
		CMD_SELFTEST =			0xaa, /* Self test */
		SELFTEST_OK =			0x55, /* Self test succeeded */

		CMD_IFTEST =			0xab, /* Interface test */
		/* Interface test result codes */
		IFTRC_OK =				0x00, /* No errors */
		IFTRC_KBD_CLK_LOW =		0x01, /* 'keyboard clock' line is stuck low */
		IFTRC_KBD_CLK_HI =		0x02, /* 'keyboard clock' line is stuck high */
		IFTRC_KBD_DATA_LOW =	0x03, /* 'keyboard data' line is stuck low */
		IFTRC_KBD_DATA_HI =		0x04, /* 'keyboard data' line is stuck high */

		CMD_DUMP =				0xac, /* Diagnostic dump */
		CMD_DISABLE_KBD =		0xad, /* Disable keyboard */
		CMD_ENABLE_KBD =		0xae, /* Enable keyboard */
		CMD_READ_INPUT =		0xc0, /* Read input port */
		CMD_READ_OUTPUT =		0xd0, /* Read output port */
		CMD_WRITE_OUTPUT =		0xd1, /* Write output port */
		CMD_READ_TEST =			0xe0, /* Read test inputs */

		/* The following commands written to REG_INPUT */
		CMD_RESET =				0xff, /* Reset */
		CMD_RESEND =			0xee, /* Resend */
		CMD_NOP =				0xf7, /* No-Operation */
		CMD_SET_DEFAULT =		0xf6, /* Set default */
		CMD_DEFAULT_DISABLE =	0xf5, /* Default disable */
		CMD_ENABLE =			0xf4, /* Enable scanning */
		CMD_SET_TYPEMATIC =		0xf3, /* Set typmatic rate/delay */
		CMD_ECHO =				0xee, /* Echo */
		CMD_SET_IND =			0xed, /* Set/reset indicators */
		/* Indicators */
		IND_SCROLL_LOCK =		0x01,
		IND_NUM_LOCK =			0x02,
		IND_CAPS_LOCK =			0x04,
	};

	static KeyCode scanMap[0x80], scanMapEsc[0x80];
	SpinLock lock;
	Handle irq;
	int escActive, esc1Active;
	int ledsPending;

	int TranslateCode(u8 code);
	int FlushOutput();
	int Command(int cmd, u8 *rpl1 = 0, int param = -1, u8 *rpl2 = 0);
	int WaitInput();
	int WaitOutput();
	IM::IsrStatus IntrHandler(Handle h);
	void Lock() { lock.Lock(); }
	void Unlock();
public:
	DeclareDevFactory();
	DeclareDevProber();

	KbdController(Type type, u32 unit, u32 classID);
	virtual ~KbdController();

	virtual int SetLeds(int leds);
};

#endif /* KBDCONTROLLER_H_ */
