/*
 * /phobos/kernel/dev/chr/KbdLayout.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef KBDLAYOUT_H_
#define KBDLAYOUT_H_
#include <sys.h>
phbSource("$Id$");

#include <dev/chr/KbdDev.h>

class KbdLayout : public Object {
public:
	enum Flags {
		F_SHIFT =		0x1,
		F_CTRL =		0x2,
		F_ALT =			0x4,
		F_CAPS =		0x8,
		F_NUM =			0x10,
	};
private:
	typedef struct {
		KbdDev::KeyCode key;
		char normal, shifted;
	} LayoutEntry;

	static LayoutEntry defLayout[];

	char map[0x80], shiftMap[0x80];

	void InitMaps(LayoutEntry *layout);
public:
	KbdLayout();
	virtual ~KbdLayout();

	virtual int Translate(KbdDev::KeyCode key, int flags, char *pChr);
};

#endif /* KBDLAYOUT_H_ */
