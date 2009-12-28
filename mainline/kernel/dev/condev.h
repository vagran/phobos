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

class ConsoleDev : public ChrDevice {
public:
	typedef enum {
		K_BACKSPACE =		8,
		K_UP =				65,
		K_DOWN =			66,
		K_RIGHT =			67,
		K_LEFT =			68,
	} Keys;
private:
	ChrDevice *inDev, *outDev;

	static void _Putc(int c, ConsoleDev *p);

protected:
	int SetInputDevice(ChrDevice *dev);
	int SetOutputDevice(ChrDevice *dev);
public:
	ConsoleDev(Type type, u32 unit, u32 classID);
	virtual ~ConsoleDev();

	virtual IOStatus Getc(u8 *c);
	virtual IOStatus Putc(u8 c);

	int VPrintf(const char *fmt, va_list va);
	int Printf(const char *fmt,...);
};

#endif /* CONDEV_H_ */
