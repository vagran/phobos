/*
 * /phobos/kernel/dev/chr/KbdDev.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include "KbdDev.h"

#define KEYCODESTR(x)	{ x, __STR(x) }
KbdDev::KeyCodeStr KbdDev::keyCodeStr[] = {
	KEYCODESTR(KEY_NONE),
	KEYCODESTR(KEY_BACKSPACE),
	KEYCODESTR(KEY_TAB),
	KEYCODESTR(KEY_ENTER),
	KEYCODESTR(KEY_LCTRL),
	KEYCODESTR(KEY_LSHIFT),
	KEYCODESTR(KEY_LALT),
	KEYCODESTR(KEY_RCTRL),
	KEYCODESTR(KEY_RSHIFT),
	KEYCODESTR(KEY_RALT),
	KEYCODESTR(KEY_LEFT),
	KEYCODESTR(KEY_RIGHT),
	KEYCODESTR(KEY_UP),
	KEYCODESTR(KEY_DOWN),
	KEYCODESTR(KEY_INSERT),
	KEYCODESTR(KEY_DELETE),
	KEYCODESTR(KEY_HOME),
	KEYCODESTR(KEY_END),
	KEYCODESTR(KEY_PAGEUP),
	KEYCODESTR(KEY_PAGEDOWN),
	KEYCODESTR(KEY_PAUSE),
	KEYCODESTR(KEY_PRTSCR),
	KEYCODESTR(KEY_SPACE),
	KEYCODESTR(KEY_ESC),
	KEYCODESTR(KEY_CAPSLOCK),
	KEYCODESTR(KEY_NUMLOCK),
	KEYCODESTR(KEY_SCROLLLOCK),
	KEYCODESTR(KEY_COMMA),
	KEYCODESTR(KEY_DOT),
	KEYCODESTR(KEY_SLASH),
	KEYCODESTR(KEY_SEMICOLON),
	KEYCODESTR(KEY_QUOTE),
	KEYCODESTR(KEY_BACKSLASH),
	KEYCODESTR(KEY_LSQUARE),
	KEYCODESTR(KEY_RSQUARE),
	KEYCODESTR(KEY_MINUS),
	KEYCODESTR(KEY_EQUAL),
	KEYCODESTR(KEY_TILDA),
	KEYCODESTR(KEY_0),
	KEYCODESTR(KEY_1),
	KEYCODESTR(KEY_2),
	KEYCODESTR(KEY_3),
	KEYCODESTR(KEY_4),
	KEYCODESTR(KEY_5),
	KEYCODESTR(KEY_6),
	KEYCODESTR(KEY_7),
	KEYCODESTR(KEY_8),
	KEYCODESTR(KEY_9),
	KEYCODESTR(KEY_A),
	KEYCODESTR(KEY_B),
	KEYCODESTR(KEY_C),
	KEYCODESTR(KEY_D),
	KEYCODESTR(KEY_E),
	KEYCODESTR(KEY_F),
	KEYCODESTR(KEY_G),
	KEYCODESTR(KEY_H),
	KEYCODESTR(KEY_I),
	KEYCODESTR(KEY_J),
	KEYCODESTR(KEY_K),
	KEYCODESTR(KEY_L),
	KEYCODESTR(KEY_M),
	KEYCODESTR(KEY_N),
	KEYCODESTR(KEY_O),
	KEYCODESTR(KEY_P),
	KEYCODESTR(KEY_Q),
	KEYCODESTR(KEY_R),
	KEYCODESTR(KEY_S),
	KEYCODESTR(KEY_T),
	KEYCODESTR(KEY_U),
	KEYCODESTR(KEY_V),
	KEYCODESTR(KEY_W),
	KEYCODESTR(KEY_X),
	KEYCODESTR(KEY_Y),
	KEYCODESTR(KEY_Z),
	KEYCODESTR(KEY_F1),
	KEYCODESTR(KEY_F2),
	KEYCODESTR(KEY_F3),
	KEYCODESTR(KEY_F4),
	KEYCODESTR(KEY_F5),
	KEYCODESTR(KEY_F6),
	KEYCODESTR(KEY_F7),
	KEYCODESTR(KEY_F8),
	KEYCODESTR(KEY_F9),
	KEYCODESTR(KEY_F10),
	KEYCODESTR(KEY_F11),
	KEYCODESTR(KEY_F12),
	KEYCODESTR(KEY_RWIN),
	KEYCODESTR(KEY_LWIN),
	KEYCODESTR(KEY_MENU),
	KEYCODESTR(KEY_NUM0),
	KEYCODESTR(KEY_NUM1),
	KEYCODESTR(KEY_NUM2),
	KEYCODESTR(KEY_NUM3),
	KEYCODESTR(KEY_NUM4),
	KEYCODESTR(KEY_NUM5),
	KEYCODESTR(KEY_NUM6),
	KEYCODESTR(KEY_NUM7),
	KEYCODESTR(KEY_NUM8),
	KEYCODESTR(KEY_NUM9),
	KEYCODESTR(KEY_NUM_SLASH),
	KEYCODESTR(KEY_NUM_MULT),
	KEYCODESTR(KEY_NUM_MINUS),
	KEYCODESTR(KEY_NUM_PLUS),
	KEYCODESTR(KEY_NUM_DOT),
	KEYCODESTR(KEY_NUM_ENTER),
};

const char *KbdDev::keyStr[0x100];

const char *
KbdDev::GetKeyStr(KeyCode key)
{
	if ((size_t)key >= SIZEOFARRAY(keyStr)) {
		return 0;
	}
	if (!keyStr[KEY_NONE]) {
		/* Initialize map */
		for (size_t i = 0; i < SIZEOFARRAY(keyCodeStr); i++) {
			keyStr[keyCodeStr[i].code] = keyCodeStr[i].str;
		}
	}
	return keyStr[key];
}

KbdDev::KbdDev(Type type, u32 unit, u32 classID) : ChrDevice(type, unit, classID)
{
	assert(KEY_MAX <= 0x80);
	memset(keyMap, 0, sizeof(keyMap));
	leds = 0;
}

KbdDev::~KbdDev()
{

}

/* Should be called with interrupts disabled */
int
KbdDev::KeyEvent(int code)
{
	fifoLock.Lock();
	fifo.Push((u8)code);
	u8 key = code & KEY_CODEMASK;
	if (code & KEYS_RELEASED) {
		keyMap[key] &= ~(KEYF_PRESSED | KEYF_AUTOREPEAT);
	} else {
		if (keyMap[key] & KEYF_PRESSED) {
			keyMap[key] |= KEYF_AUTOREPEAT;
		} else {
			keyMap[key] |= KEYF_PRESSED;
		}
	}
	fifoLock.Unlock();
	return 0;
}

u32
KbdDev::GetWaitChannel(Operation op)
{
	if (op == OP_READ) {
		return (u32)&fifo;
	}
	return 0;
}

Device::IOStatus
KbdDev::Getc(u8 *c)
{
	u64 cpl = im->SetPL(IM::IP_KBD);
	fifoLock.Lock();
	IOStatus s = fifo.Pull(c) ? IOS_NODATA : IOS_OK;
	fifoLock.Unlock();
	im->RestorePL(cpl);
	return s;
}

Device::IOStatus
KbdDev::OpAvailable(Operation op)
{
	if (op == OP_READ) {
		return fifo.GetLenth() ? IOS_OK : IOS_NODATA;
	}
	return IOS_NOTSPRT;
}
