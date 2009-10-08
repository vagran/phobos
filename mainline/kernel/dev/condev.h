/*
 * /kernel/dev/condev.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef CONDEV_H_
#define CONDEV_H_
#include <sys.h>
phbSource("$Id$");

#include <dev/device.h>

class ConsoleDev : public Device {
public:

private:
	ChrDevice *inDev, *outDev;

	static void _Putc(int c, ConsoleDev *p);

protected:
	int SetInputDevice(ChrDevice *dev);
	int SetOutputDevice(ChrDevice *dev);
public:
	ConsoleDev(Type type, u32 unit, u32 classID);

	void Putc(u8 c);
	u8 Getc();

	int VPrintf(char *fmt, va_list va);
	int Printf(char *fmt,...);
};

#endif /* CONDEV_H_ */
