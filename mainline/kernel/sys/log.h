/*
 * /kernel/sys/log.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef LOG_H_
#define LOG_H_
#include <sys.h>
phbSource("$Id$");

class Log {
public:
	typedef enum {
		LOG_DEBUG,
		LOG_INFO,
		LOG_WARNING,
		LOG_ERROR,
		LOG_MAX
	} Level;
private:
	Level curLevel;
	SpinLock lock;
	static const char *levelStr[];
public:
	Log();

	int WriteV(const char *funcName, int line, Level level,
		const char *fmt, va_list args) __format(printf, 5, 0);
	int Write(const char *funcName, int line, Level level,
		const char *fmt,...) __format(printf, 5, 6);
};

extern Log *log;

#define KLOG_DEBUG		Log::LOG_DEBUG
#define KLOG_INFO		Log::LOG_INFO
#define KLOG_WARNING	Log::LOG_WARNING
#define KLOG_ERROR		Log::LOG_ERROR

#define klog(level, fmt,...) { if (log) log->Write(__func__, __LINE__, level, fmt, ## __VA_ARGS__); }
#define klogv(level, fmt, args) { if (log) log->WriteV(__func__, __LINE__, level, fmt, args); }

#endif /* LOG_H_ */
