/*
 * /phobos/kernel/dev/chr/KbdController.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include "KbdController.h"

DefineDevFactory(KbdController);

RegProbeDevClass(KbdController, "8042kbd", Device::T_CHAR, "8042 keyboard controller");

/* Scan codes map */
KbdDev::KeyCode KbdController::scanMap[0x80] = {
/* 0x00 */	KEY_NONE,	KEY_ESC,	KEY_1,		KEY_2,		KEY_3,		KEY_4,		KEY_5,		KEY_6,
/* 0x08 */	KEY_7,		KEY_8,		KEY_9,		KEY_0,		KEY_MINUS,	KEY_EQUAL,	KEY_BACKSPACE,	KEY_TAB,
/* 0x10 */	KEY_Q,		KEY_W,		KEY_E,		KEY_R,		KEY_T,		KEY_Y,		KEY_U,		KEY_I,
/* 0x18 */	KEY_O,		KEY_P,		KEY_LSQUARE,KEY_RSQUARE,KEY_ENTER,	KEY_LCTRL,	KEY_A,		KEY_S,
/* 0x20 */	KEY_D,		KEY_F,		KEY_G,		KEY_H,		KEY_J,		KEY_K,		KEY_L,		KEY_SEMICOLON,
/* 0x28 */	KEY_QUOTE,	KEY_TILDA,	KEY_LSHIFT,	KEY_BACKSLASH, KEY_Z,	KEY_X,		KEY_C,		KEY_V,
/* 0x30 */	KEY_B,		KEY_N,		KEY_M,		KEY_COMMA,	KEY_DOT,	KEY_SLASH,	KEY_RSHIFT,	KEY_NUM_MULT,
/* 0x38 */	KEY_LALT,	KEY_SPACE,	KEY_CAPSLOCK, KEY_F1,		KEY_F2,		KEY_F3,		KEY_F4,		KEY_F5,
/* 0x40 */	KEY_F6,		KEY_F7,		KEY_F8,		KEY_F9,		KEY_F10,	KEY_PAUSE,	KEY_SCROLLLOCK,	KEY_NUM7,
/* 0x48 */	KEY_NUM8,	KEY_NUM9,	KEY_NUM_MINUS, KEY_NUM4, KEY_NUM5,	KEY_NUM6,	KEY_NUM_PLUS, KEY_NUM1,
/* 0x50 */	KEY_NUM2,	KEY_NUM3,	KEY_NUM0,	KEY_NUM_DOT, KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_F11,
/* 0x58 */	KEY_F12,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,
/* 0x60 */	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,
/* 0x68 */	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,
/* 0x70 */	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,
/* 0x78 */	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,
};

/* Escaped by 0xe0 prefix scan codes */
KbdDev::KeyCode KbdController::scanMapEsc[0x80] = {
/* 0x00 */	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,
/* 0x08 */	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,
/* 0x10 */	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,
/* 0x18 */	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NUM_ENTER, KEY_RCTRL, KEY_NONE,	KEY_NONE,
/* 0x20 */	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,
/* 0x28 */	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,
/* 0x30 */	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NUM_SLASH, KEY_NONE, KEY_PRTSCR,
/* 0x38 */	KEY_RALT,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,
/* 0x40 */	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_HOME,
/* 0x48 */	KEY_UP,		KEY_PAGEUP,	KEY_NONE,	KEY_LEFT,	KEY_NONE,	KEY_RIGHT,	KEY_NONE,	KEY_END,
/* 0x50 */	KEY_DOWN,	KEY_PAGEDOWN, KEY_INSERT, KEY_DELETE, KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,
/* 0x58 */	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_LWIN,	KEY_RWIN,	KEY_MENU,	KEY_NONE,	KEY_NONE,
/* 0x60 */	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,
/* 0x68 */	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,
/* 0x70 */	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,
/* 0x78 */	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,	KEY_NONE,
};

DefineDevProber(KbdController)
{
	int rc = -1;
	if (lastUnit != -1) {
		/* Only one unit is supported */
		return -1;
	}
	u32 intr = IM::DisableIntr();
	do {
		int to;

		/* flush output buffer */
		for (to = 1000; to > 0; to--) {
			if (!(inb(REG_STATUS) & REG_STATUS_OBF)) {
				break;
			}
			inb(REG_OUTPUT);
		}
		/* Send self test command */
		for (to = 1000; to > 0; to--) {
			if (!(inb(REG_STATUS) & REG_STATUS_IBF)) {
				break;
			}
			CPU::Delay(10);
		}
		if (!to) {
			break;
		}
		outb(REG_COMMAND, CMD_SELFTEST);

		/* get result */
		for (to = 1000; to > 0; to--) {
			if (inb(REG_STATUS) & REG_STATUS_OBF) {
				break;
			}
			CPU::Delay(10);
		}
		if (!to) {
			break;
		}
		u8 res = inb(REG_OUTPUT);
		if (res != SELFTEST_OK) {
			break;
		}
		rc = 0;
	} while (0);
	IM::RestoreIntr(intr);
	return rc;
}

KbdController::KbdController(Type type, u32 unit, u32 classID) :
	KbdDev(type, unit, classID)
{
	u8 rpl;

	escActive = 0;
	esc1Active = 0;

	/* Test interface */
	if (Command(CMD_IFTEST, &rpl)) {
		klog(KLOG_WARNING, "Keyboard controller: command failed");
		return;
	}
	if (rpl != IFTRC_OK) {
		klog(KLOG_WARNING, "Keyboard controller: interface test failed, "
			"device disabled");
		return;
	}

	/* Set mode */
	if (Command(CMD_READ_COMMAND, &rpl)) {
		klog(KLOG_WARNING, "Keyboard controller: command failed");
		return;
	}
	rpl |= CMDB_IBM_COMP | CMDB_IBM_MODE | CMDB_OBFI;
	rpl &= ~CMDB_DISABLE_KBD;
	if (Command(CMD_WRITE_COMMAND, 0, rpl)) {
		klog(KLOG_WARNING, "Keyboard controller: command failed");
		return;
	}

	/* Reset keyboard */
	if (Command(CMD_NONE, 0, CMD_RESET, &rpl)) {
		klog(KLOG_WARNING, "Keyboard controller: command failed");
		return;
	}
	if (rpl != ACK) {
		klog(KLOG_WARNING, "Keyboard controller: keyboard reset failed");
		return;
	}

	/* Send echo */
	if (Command(CMD_NONE, 0, CMD_ECHO, &rpl)) {
		klog(KLOG_WARNING, "Keyboard controller: command failed");
		return;
	}
	if (rpl != ECHO) {
		klog(KLOG_WARNING, "Keyboard controller: echo request failed");
		return;
	}

	/* Enable scanning */
	if (Command(CMD_NONE, 0, CMD_ENABLE, &rpl)) {
		klog(KLOG_WARNING, "Keyboard controller: command failed");
		return;
	}
	if (rpl != ACK) {
		klog(KLOG_WARNING, "Keyboard controller: scanning enabling failed");
		return;
	}

	/* Turn off leds */
	SetLeds(0);

	fifo.Reset();

	irq = im->AllocateHwirq(this, (IM::ISR)&KbdController::IntrHandler,
		PIC::IRQ_KBD, IM::AF_SPEC | IM::AF_EXCLUSIVE, IM::IP_KBD);
	ensure(irq);
	im->HwEnable(PIC::IRQ_KBD);

	devState = S_UP;
}

KbdController::~KbdController()
{

}

int
KbdController::TranslateCode(u8 code)
{
	/* Drop keys with extended escape prefix */
	if (code == CODE_ESC1) {
		esc1Active = 1;
		return 0;
	}
	if (esc1Active) {
		esc1Active = 0;
		return 0;
	}

	if (code == CODE_ESC) {
		escActive = 1;
		return 0;
	}
	int event;
	if (escActive) {
		event = scanMapEsc[code & CODE_MASK];
		escActive = 0;
	} else {
		event = scanMap[code & CODE_MASK];
	}
	if (event == KEY_NONE) {
		return 0;
	}
	if (code & CODE_RELEASED) {
		event |= KEYS_RELEASED;
	}
	return KeyEvent(event);
}

void
KbdController::Unlock()
{
	lock.Unlock();
	if (ledsPending) {
		ledsPending = 0;
		SetLeds(leds);
	}
}

int
KbdController::FlushOutput()
{
	u32 intr = IM::DisableIntr();
	Lock();
	while (inb(REG_STATUS) & REG_STATUS_OBF) {
		TranslateCode(inb(REG_OUTPUT));
	}
	Unlock();
	IM::RestoreIntr(intr);
	if (fifo.GetLenth()) {
		Notify(OP_READ);
	}
	return 0;
}

/* must be called with lock and disabled interrupts */
int
KbdController::WaitInput()
{
	int to;
	for (to = 1000; to > 0; to--) {
		if (!(inb(REG_STATUS) & REG_STATUS_IBF)) {
			break;
		}
		CPU::Delay(10);
	}
	if (!to) {
		return -1;
	}
	return 0;
}

/* must be called with lock and disabled interrupts */
int
KbdController::WaitOutput()
{
	int to;
	for (to = 1000; to > 0; to--) {
		if (inb(REG_STATUS) & REG_STATUS_OBF) {
			break;
		}
		CPU::Delay(10);
	}
	if (!to) {
		return -1;
	}
	return 0;
}

int
KbdController::Command(int cmd, u8 *rpl1, int param, u8 *rpl2)
{
	int rc = -1;

	FlushOutput();

	u32 intr = IM::DisableIntr();
	Lock();
	do {
		if (cmd != CMD_NONE) {
			assert(cmd >= 0 && cmd <= 0xff);
			if (WaitInput()) {
				break;
			}
			outb(REG_COMMAND, cmd);
			if (rpl1) {
				if (WaitOutput()) {
					break;
				}
				*rpl1 = inb(REG_OUTPUT);
			}
		}

		if (param != -1) {
			assert(param >= 0 && param <= 0xff);
			if (WaitInput()) {
				break;
			}
			outb(REG_INPUT, param);
			if (rpl2) {
				if (WaitOutput()) {
					break;
				}
				*rpl2 = inb(REG_OUTPUT);
			}
		}

		rc = 0;
	} while (0);

	Unlock();
	IM::RestoreIntr(intr);
	return rc;
}

int
KbdController::SetLeds(int leds)
{
	this->leds = leds;
	if (lock) {
		ledsPending = 1;
		return 0;
	}

	int ind = 0;
	if (leds & LED_SCROLL) {
		ind |= IND_SCROLL_LOCK;
	}
	if (leds & LED_NUM) {
		ind |= IND_NUM_LOCK;
	}
	if (leds & LED_CAPS) {
		ind |= IND_CAPS_LOCK;
	}

	u8 rpl;
	if (Command(CMD_NONE, 0, CMD_SET_IND, &rpl)) {
		klog(KLOG_WARNING, "Keyboard controller: SetLeds command failed");
		return -1;
	}
	if (rpl != ACK) {
		klog(KLOG_WARNING, "Keyboard controller: SetLeds command bad response");
		return -1;
	}
	if (Command(CMD_NONE, 0, ind, &rpl)) {
		klog(KLOG_WARNING, "Keyboard controller: SetLeds command (option) failed");
		return -1;
	}
	if (rpl != ACK) {
		klog(KLOG_WARNING, "Keyboard controller: SetLeds command (option) bad response");
		return -1;
	}
	return 0;
}

IM::IsrStatus
KbdController::IntrHandler(Handle h)
{
	FlushOutput();
	return IM::IS_PROCESSED;
}
