/*
 * /kernel/dev/chr/syscons.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#ifndef SYSCONS_H_
#define SYSCONS_H_
#include <sys.h>
phbSource("$Id$");

#include <dev/condev.h>

class SysConsole : public ConsoleDev {
private:

public:
	DeclareDevFactory();
	SysConsole(Type type, u32 unit, u32 classID);
	virtual ~SysConsole();
};

extern SysConsole *sysCons;

#endif /* SYSCONS_H_ */
