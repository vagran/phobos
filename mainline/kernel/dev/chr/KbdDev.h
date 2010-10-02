/*
 * /phobos/kernel/dev/chr/KbdDev.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef KBDDEV_H_
#define KBDDEV_H_
#include <sys.h>
phbSource("$Id$");

/* Base class for generic keyboard device */

class KbdDev : public ChrDevice {
public:
	/* Keys flags in keys map */
	enum KeyFlags {
		KEYF_PRESSED =		0x1,
		KEYF_AUTOREPEAT =	0x2,
	};

	/* Status bits in scan codes */
	enum KeyStatus {
		KEYS_RELEASED =		0x80,
	};

	enum Leds {
		LED_SCROLL =	0x1,
		LED_NUM =		0x2,
		LED_CAPS =		0x4,
	};

	enum KeyCode {
		KEY_NONE =		0x00,

		KEY_CODEMASK =	0x7f,

		KEY_BACKSPACE = 0x8,
		KEY_TAB = '\t',
		KEY_ENTER = '\r',

		KEY_LCTRL,
		KEY_LSHIFT,
		KEY_LALT,
		KEY_RCTRL,
		KEY_RSHIFT,
		KEY_RALT,

		KEY_LEFT,
		KEY_RIGHT,
		KEY_UP,
		KEY_DOWN,

		KEY_INSERT,
		KEY_DELETE,
		KEY_HOME,
		KEY_END,
		KEY_PAGEUP,
		KEY_PAGEDOWN,

		KEY_PAUSE,
		KEY_PRTSCR,

		KEY_SPACE = ' ',

		KEY_ESC,

		KEY_CAPSLOCK,
		KEY_NUMLOCK,
		KEY_SCROLLLOCK,

		KEY_COMMA,
		KEY_DOT,
		KEY_SLASH,
		KEY_SEMICOLON,
		KEY_QUOTE,
		KEY_BACKSLASH,
		KEY_LSQUARE,
		KEY_RSQUARE,
		KEY_MINUS,
		KEY_EQUAL,
		KEY_TILDA,

		KEY_0 = '0',
		KEY_1,
		KEY_2,
		KEY_3,
		KEY_4,
		KEY_5,
		KEY_6,
		KEY_7,
		KEY_8,
		KEY_9,

		KEY_A = 'A',
		KEY_B,
		KEY_C,
		KEY_D,
		KEY_E,
		KEY_F,
		KEY_G,
		KEY_H,
		KEY_I,
		KEY_J,
		KEY_K,
		KEY_L,
		KEY_M,
		KEY_N,
		KEY_O,
		KEY_P,
		KEY_Q,
		KEY_R,
		KEY_S,
		KEY_T,
		KEY_U,
		KEY_V,
		KEY_W,
		KEY_X,
		KEY_Y,
		KEY_Z,

		KEY_F1,
		KEY_F2,
		KEY_F3,
		KEY_F4,
		KEY_F5,
		KEY_F6,
		KEY_F7,
		KEY_F8,
		KEY_F9,
		KEY_F10,
		KEY_F11,
		KEY_F12,

		KEY_RWIN,
		KEY_LWIN,
		KEY_MENU,

		KEY_NUM0,
		KEY_NUM1,
		KEY_NUM2,
		KEY_NUM3,
		KEY_NUM4,
		KEY_NUM5,
		KEY_NUM6,
		KEY_NUM7,
		KEY_NUM8,
		KEY_NUM9,
		KEY_NUM_SLASH,
		KEY_NUM_MULT,
		KEY_NUM_MINUS,
		KEY_NUM_PLUS,
		KEY_NUM_DOT,
		KEY_NUM_ENTER,

		KEY_MAX
	};

	struct KeyCodeStr {
		KeyCode code;
		const char *str;
	};
	static KeyCodeStr keyCodeStr[];
	static const char *keyStr[0x100];
	static const char *GetKeyStr(KeyCode key);
private:

protected:
	enum {
		FIFO_SIZE =		1024,
	};
	SpinLock fifoLock;
	Fifo<u8, FIFO_SIZE, 1> fifo;

	u8 keyMap[0x100];
	int leds;

	int KeyEvent(int code);
public:
	KbdDev(Type type, u32 unit, u32 classID);
	virtual ~KbdDev();

	IOStatus Getc(u8 *c);

	inline u32 GetKeyStatus(KeyCode key) {
		assert((size_t)key < sizeof(keyMap) / sizeof(keyMap[0]));
		return keyMap[key];
	}

	virtual int SetLeds(int leds) { return -1; }
	virtual int GetLeds() { return leds; }
	virtual u32 GetWaitChannel(Operation op);
};

#endif /* KBDDEV_H_ */
