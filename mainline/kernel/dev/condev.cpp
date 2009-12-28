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
	ChrDevice(type, unit, classID)
{
	inDev = 0;
	outDev = 0;
}

ConsoleDev::~ConsoleDev()
{

}

Device::IOStatus
ConsoleDev::Putc(u8 c)
{
	if (!outDev) {
		return IOS_NOTSPRT;
	}
	return outDev->Putc(c);
}

Device::IOStatus
ConsoleDev::Getc(u8 *c)
{
	if (!inDev) {
		return IOS_NOTSPRT;
	}
	return inDev->Getc(c);
}

int
ConsoleDev::Printf(const char *fmt,...)
{
	va_list va;
	va_start(va, fmt);
	return VPrintf(fmt, va);
}

int
ConsoleDev::VPrintf(const char *fmt, va_list va)
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
