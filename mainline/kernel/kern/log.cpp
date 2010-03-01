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
Log::WriteV(const char *funcName, int line, Level level, const char *fmt,
	va_list args)
{
	if (level >= curLevel) {
		assert(level < LOG_MAX);
		printf("[%s] %s@%d: ", levelStr[level], funcName, line);
		vprintf(fmt, args);
		printf("\n");
		lock.Lock();
		/* update log */
		//notimpl
		lock.Unlock();
	}
	return 0;
}

int
Log::Write(const char *funcName, int line, Level level, const char *fmt,...)
{
	va_list va;
	va_start(va, fmt);
	return WriteV(funcName, line, level, fmt, va);
}
