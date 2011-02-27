/*
 * /kernel/kern/log.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#include <sys.h>
phbSource("$Id$");

void
_klogv(const char *func, int line, Log::Level level, const char *fmt, va_list args)
{
	if (log) {
		log->WriteV(func, line, level, fmt, args);
	} else {
		printf("[%d] %s@%d: ", level, func, line);
		vprintf(fmt, args);
		printf("\n");
	}
}

void
_klog(const char *func, int line, Log::Level level, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	_klogv(func, line, level, fmt, args);
}

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
