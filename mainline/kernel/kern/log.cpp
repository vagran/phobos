/*
 * /kernel/kern/log.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

Log *log = 0;

const char *Log::levelStr[] = {"DEBUG", "INFO", "WARNING", "ERROR"};

Log::Log()
{
	curLevel = LOG_DEBUG;
}

int
Log::Write(const char *funcName, int line, Level level, const char *fmt,...)
{
	lock.Lock();
	if (level >= curLevel) {
		va_list va;
		va_start(va, fmt);
		printf("[%s] %s@%d: ", levelStr[level], funcName, line);
		vprintf(fmt, va);
		printf("\n");
	}
	lock.Unlock();
	return 0;
}
