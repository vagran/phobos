/*
 * /phobos/kernel/sys/error.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#ifndef ERROR_H_
#define ERROR_H_
#include <sys.h>
phbSource("$Id$");

class Error : public Object {
public:
	enum ErrorCode {
		E_SUCCESS = 0,			/* No error */
		E_FAULT = -1,			/* Unspecified error */
		E_NOMEM = -32767,		/* No memory */
		E_IO,					/* I/O operations error */
		E_INVAL,				/* Invalid parameters */
		E_THREAD_FAULT,			/* Thread fault raised */
		E_INVAL_GATEOBJ,		/* Invalid gate object pointer passed to system call */
		E_OP_NOTSUPPORTED,		/* Operation not supported */
		E_EXISTS,				/* Object already exists */
		E_NOTFOUND,				/* Object not found */
	};
private:
	enum {
		MAX_FREE_ENTRIES =	8,
	};

	typedef struct {
		ErrorCode code;
		const char *designation;
		const char *desc;
	} ErrorDef;

	static ErrorDef errorDefs[];

	typedef struct {
		ListEntry list;
		ErrorCode code;
		const char *function;
		int line;
		KString desc; /* Additional information string */
	} ErrorEntry;

	ListHead freeEntries; /* Cached list of free entries */
	int numFreeEntries;
	ListHead entries; /* Current errors trace list */
	int numEntries;
	KString str; /* human readable representation, cached value */
	int strValid; /* cached string representation valid */

	ErrorEntry *CreateEntry();
	void DestroyEntry(ErrorEntry *e);
	ErrorEntry *AddEntry();
	void FreeEntry(ErrorEntry *e);
	ErrorDef *GetDef(ErrorCode code);
	ErrorEntry *GetCurrent();
public:
	Error();
	~Error();

	int RaiseError(const char *function, int line, ErrorCode code,
		const char *fmt = 0, ...) __format(printf, 5, 6);
	int Reset();
	ErrorCode GetCode();
	int GetStr(char *buf, int bufLen);
};

#endif /* ERROR_H_ */
