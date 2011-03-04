/*
 * /phobos/kernel/dev/chr/KbdLayout.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#include <sys.h>
phbSource("$Id$");

#include <dev/chr/KbdLayout.h>

#define LE(key, normal, shifted) { KbdDev::key, normal, shifted }

/* Default keyboard layout */
KbdLayout::LayoutEntry KbdLayout::defLayout[] = {
	LE(KEY_BACKSPACE,	'\x08',	'\x08'),
	LE(KEY_TAB,			'\t',	'\t'),
	LE(KEY_ENTER,		'\n',	'\n'),
	LE(KEY_SPACE,		' ',	' '),
	LE(KEY_COMMA,		',',	'<'),
	LE(KEY_DOT,			'.',	'>'),
	LE(KEY_SLASH,		'/',	'?'),
	LE(KEY_SEMICOLON,	';',	':'),
	LE(KEY_QUOTE,		'\'',	'"'),
	LE(KEY_BACKSLASH,	'\\',	'|'),
	LE(KEY_LSQUARE,		'[',	'{'),
	LE(KEY_RSQUARE,		']',	'}'),
	LE(KEY_MINUS,		'-',	'_'),
	LE(KEY_EQUAL,		'=',	'+'),
	LE(KEY_TILDA,		'`',	'~'),
	LE(KEY_0,			'0',	')'),
	LE(KEY_1,			'1',	'!'),
	LE(KEY_2,			'2',	'@'),
	LE(KEY_3,			'3',	'#'),
	LE(KEY_4,			'4',	'$'),
	LE(KEY_5,			'5',	'%'),
	LE(KEY_6,			'6',	'^'),
	LE(KEY_7,			'7',	'&'),
	LE(KEY_8,			'8',	'*'),
	LE(KEY_9,			'9',	'('),
	LE(KEY_A,			'a',	'A'),
	LE(KEY_B,			'b',	'B'),
	LE(KEY_C,			'c',	'C'),
	LE(KEY_D,			'd',	'D'),
	LE(KEY_E,			'e',	'E'),
	LE(KEY_F,			'f',	'F'),
	LE(KEY_G,			'g',	'G'),
	LE(KEY_H,			'h',	'H'),
	LE(KEY_I,			'i',	'I'),
	LE(KEY_J,			'j',	'J'),
	LE(KEY_K,			'k',	'K'),
	LE(KEY_L,			'l',	'L'),
	LE(KEY_M,			'm',	'M'),
	LE(KEY_N,			'n',	'N'),
	LE(KEY_O,			'o',	'O'),
	LE(KEY_P,			'p',	'P'),
	LE(KEY_Q,			'q',	'Q'),
	LE(KEY_R,			'r',	'R'),
	LE(KEY_S,			's',	'S'),
	LE(KEY_T,			't',	'T'),
	LE(KEY_U,			'u',	'U'),
	LE(KEY_V,			'v',	'V'),
	LE(KEY_W,			'w',	'W'),
	LE(KEY_X,			'x',	'X'),
	LE(KEY_Y,			'y',	'Y'),
	LE(KEY_Z,			'z',	'Z'),
	LE(KEY_NUM0,		'0',	'0'),
	LE(KEY_NUM1,		'1',	'1'),
	LE(KEY_NUM2,		'2',	'2'),
	LE(KEY_NUM3,		'3',	'3'),
	LE(KEY_NUM4,		'4',	'4'),
	LE(KEY_NUM5,		'5',	'5'),
	LE(KEY_NUM6,		'6',	'6'),
	LE(KEY_NUM7,		'7',	'7'),
	LE(KEY_NUM8,		'8',	'8'),
	LE(KEY_NUM9,		'9',	'9'),
	LE(KEY_NUM_SLASH,	'/',	'/'),
	LE(KEY_NUM_MULT,	'*',	'*'),
	LE(KEY_NUM_MINUS,	'-',	'-'),
	LE(KEY_NUM_PLUS,	'+',	'+'),
	LE(KEY_NUM_DOT,		'.',	'.'),
	LE(KEY_NUM_ENTER,	'\n',	'\n'),

	LE(KEY_NONE,		'\0',	'\0')
};

KbdLayout::KbdLayout()
{
	InitMaps(defLayout);
}

KbdLayout::~KbdLayout()
{

}

void
KbdLayout::InitMaps(LayoutEntry *layout)
{
	memset(map, 0, sizeof(map));
	memset(shiftMap, 0, sizeof(shiftMap));
	while (layout->key != KbdDev::KEY_NONE) {
		map[layout->key] = layout->normal;
		shiftMap[layout->key] = layout->shifted;
		layout++;
	}
}

int
KbdLayout::Translate(KbdDev::KeyCode key, int flags, char *pChr)
{
	if (key >= KbdDev::KEY_MAX) {
		return -1;
	}
	if (flags & F_CTRL) {
		if (key >= KbdDev::KEY_A && key <= KbdDev::KEY_Z) {
			*pChr = 1 + key - KbdDev::KEY_A;
			return 0;
		}
		switch (key) {
		case KbdDev::KEY_LSQUARE:
			*pChr = '\x1b';
			break;
		case KbdDev::KEY_BACKSLASH:
			*pChr = '\x1c';
			break;
		case KbdDev::KEY_RSQUARE:
			*pChr = '\x1d';
			break;
		default:
			return -1;
		}
		return 0;
	}
	char chr;
	if (flags & F_SHIFT) {
		chr = shiftMap[key];
	} else {
		chr = map[key];
	}
	if (!chr) {
		return -1;
	}
	*pChr = chr;
	return 0;
}
