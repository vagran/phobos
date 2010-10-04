/*
 * /phobos/kernel/dev/chr/KbdConsole.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef KBDCONSOLE_H_
#define KBDCONSOLE_H_
#include <sys.h>
phbSource("$Id$");

#include <dev/chr/KbdDev.h>
#include <dev/chr/KbdLayout.h>

class KbdConsole : public ChrDevice {
private:
	enum {
		FIFO_SIZE =		0x4000,

	};

	SpinLock fifoLock;
	Fifo<char, FIFO_SIZE> fifo;
	ChrDevice *inDev;
	KbdLayout layout;
	int modFlagsR, modFlagsL;

	int EventHandler(ChrDevice *dev, Operation op);
public:
	DeclareDevFactory();

	KbdConsole(Type type, u32 unit, u32 classID);
	virtual ~KbdConsole();

	virtual int SetInputDevice(KbdDev *dev);
	virtual u32 GetWaitChannel(Operation op);
	virtual IOStatus Getc(u8 *c);
	virtual IOStatus OpAvailable(Operation op);
};

#endif /* KBDCONSOLE_H_ */
