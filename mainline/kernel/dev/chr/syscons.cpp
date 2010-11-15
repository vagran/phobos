/*
 * /kernel/dev/chr/syscons.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <dev/chr/syscons.h>

DefineDevFactory(SysConsole);

RegDevClass(SysConsole, "syscons", Device::T_CHAR, "System console");

/* default system console */
SysConsole *sysCons;

/* System console device */

SysConsole::SysConsole(Type type, u32 unit, u32 classID) :
	ConsoleDev(type, unit, classID)
{
	devState = S_UP;
}

SysConsole::~SysConsole()
{
}
