/*
 * /kernel/dev/chr/console.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <dev/chr/console.h>

int
Console::Printf(char *fmt,...)
{
	va_list va;
	va_start(va, fmt);
	return VPrintf(fmt, va);
}

int
Console::VPrintf(char *fmt, va_list va)
{
	return kvprintf(fmt, (PutcFunc)_Putc, this, 10, va);
}

void
Console::_Putc(int c, Console *p)
{
	p->Putc((u8)c);
}
