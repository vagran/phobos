/*
 * /kernel/dev/chr/console.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef CONSOLE_H_
#define CONSOLE_H_
#include <sys.h>
phbSource("$Id$");

class Console {
private:
	static void _Putc(int c, Console *p);

public:
	virtual void Putc(u8 c) = 0;
	virtual u8 Getc() = 0;

	int VPrintf(char *fmt, va_list va);
	int Printf(char *fmt,...);
};

#endif /* CONSOLE_H_ */
