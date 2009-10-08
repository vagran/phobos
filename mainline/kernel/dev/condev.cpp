/*
 * /kernel/dev/condev.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <dev/condev.h>

ConsoleDev::ConsoleDev(Type type, u32 unit, u32 classID) :
	Device(type, unit, classID)
{
	inDev = 0;
	outDev = 0;
}

void
ConsoleDev::Putc(u8 c)
{
	if (!outDev) {
		return;
	}
	outDev->Putc(c);
}

u8
ConsoleDev::Getc()
{
	if (!inDev) {
		return 0;
	}
	u8 c;
	if (inDev->Getc(&c) != IOS_OK) {
		return 0;
	}
	return c;
}

int
ConsoleDev::Printf(char *fmt,...)
{
	va_list va;
	va_start(va, fmt);
	return VPrintf(fmt, va);
}

int
ConsoleDev::VPrintf(char *fmt, va_list va)
{
	return kvprintf(fmt, (PutcFunc)_Putc, this, 10, va);
}

void
ConsoleDev::_Putc(int c, ConsoleDev *p)
{
	p->Putc((u8)c);
}

int
ConsoleDev::SetInputDevice(ChrDevice *dev)
{
	inDev = dev;
	return 0;
}

int
ConsoleDev::SetOutputDevice(ChrDevice *dev)
{
	outDev = dev;
	return 0;
}
