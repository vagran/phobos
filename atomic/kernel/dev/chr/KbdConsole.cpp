/*
 * /phobos/kernel/dev/chr/KbdConsole.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#include <sys.h>
phbSource("$Id$");

#include <dev/chr/KbdConsole.h>

DefineDevFactory(KbdConsole);

RegDevClass(KbdConsole, "kbdcons", Device::T_CHAR, "Keyboard console");

KbdConsole::KbdConsole(Type type, u32 unit, u32 classID) :
	ChrDevice(type, unit, classID)
{
	inDev = 0;
	modFlagsR = 0;
	modFlagsL = 0;
	devState = S_UP;
}

KbdConsole::~KbdConsole()
{
	if (inDev) {
		inDev->RegisterCallback(OP_READ, 0, 0);
		inDev->Release();
	}
}

int
KbdConsole::SetInputDevice(KbdDev *dev)
{
	ChrDevice *oldDev = inDev;
	dev->AddRef();
	inDev = dev;
	if (oldDev) {
		oldDev->RegisterCallback(OP_READ, 0, 0);
		oldDev->Release();
	}
	modFlagsR = 0;
	modFlagsL = 0;
	inDev->RegisterCallback(OP_READ, (EventCallback)&KbdConsole::EventHandler, this);
	return 0;
}

waitid_t
KbdConsole::GetWaitChannel(Operation op)
{
	if (op == OP_READ) {
		return (waitid_t)&fifo;
	}
	return 0;
}

int
KbdConsole::EventHandler(ChrDevice *dev, Operation op)
{
	u8 c;
	while (dev->Getc(&c) == IOS_OK) {
		KbdDev::KeyCode key = (KbdDev::KeyCode)(c & KbdDev::KEY_CODEMASK);
		if (c & KbdDev::KEYS_RELEASED) {
			switch (key) {
			case KbdDev::KEY_RSHIFT:
				modFlagsR &= ~KbdLayout::F_SHIFT;
				break;
			case KbdDev::KEY_RCTRL:
				modFlagsR &= ~KbdLayout::F_CTRL;
				break;
			case KbdDev::KEY_RALT:
				modFlagsR &= ~KbdLayout::F_ALT;
				break;
			case KbdDev::KEY_LSHIFT:
				modFlagsL &= ~KbdLayout::F_SHIFT;
				break;
			case KbdDev::KEY_LCTRL:
				modFlagsL &= ~KbdLayout::F_CTRL;
				break;
			case KbdDev::KEY_LALT:
				modFlagsL &= ~KbdLayout::F_ALT;
				break;
			default:
				break;
			}
			continue;
		}

		switch (key) {
		case KbdDev::KEY_RSHIFT:
			modFlagsR |= KbdLayout::F_SHIFT;
			continue;
		case KbdDev::KEY_RCTRL:
			modFlagsR |= KbdLayout::F_CTRL;
			continue;
		case KbdDev::KEY_RALT:
			modFlagsR |= KbdLayout::F_ALT;
			continue;
		case KbdDev::KEY_LSHIFT:
			modFlagsL |= KbdLayout::F_SHIFT;
			continue;
		case KbdDev::KEY_LCTRL:
			modFlagsL |= KbdLayout::F_CTRL;
			continue;
		case KbdDev::KEY_LALT:
			modFlagsL |= KbdLayout::F_ALT;
			continue;
		default:
			break;
		}

		char chr;
		if (layout.Translate(key, modFlagsR | modFlagsL, &chr)) {
			continue;
		}
		/* have new character */
		fifo.Push(chr);
	}
	if (fifo.GetLenth()) {
		Notify(OP_READ);
	}
	return 0;
}

Device::IOStatus
KbdConsole::Getc(u8 *c)
{
	u64 cpl = im->SetPL(IM::IP_KBD);
	fifoLock.Lock();
	IOStatus s = fifo.Pull((char *)c) ? IOS_NODATA : IOS_OK;
	fifoLock.Unlock();
	im->RestorePL(cpl);
	return s;
}

Device::IOStatus
KbdConsole::OpAvailable(Operation op)
{
	if (op == OP_READ) {
		return fifo.GetLenth() ? IOS_OK : IOS_NODATA;
	}
	return IOS_NOTSPRT;
}
