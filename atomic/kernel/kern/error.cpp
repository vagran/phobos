/*
 * /phobos/kernel/kern/error.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#include <sys.h>
phbSource("$Id$");

#define ERROR_DEF(code, desc)	{code, #code, desc}

Error::ErrorDef Error::errorDefs[] = {
	ERROR_DEF(E_SUCCESS,			"No error"),
	ERROR_DEF(E_FAULT,				"Unspecified fault"),
	ERROR_DEF(E_NOMEM,				"Memory allocation fault"),
	ERROR_DEF(E_IO,					"I/O operation fault"),
	ERROR_DEF(E_INVAL,				"Invalid parameters"),
	ERROR_DEF(E_THREAD_FAULT,		"Thread fault raised"),
	ERROR_DEF(E_INVAL_GATEOBJ,		"Invalid gate object pointer passed to system call"),
	ERROR_DEF(E_OP_NOTSUPPORTED,	"Operation not supported"),
	ERROR_DEF(E_EXISTS,				"Object already exists"),
	ERROR_DEF(E_NOTFOUND,			"Object not found"),
};

Error::Error()
{
	LIST_INIT(freeEntries);
	numFreeEntries = 0;
	LIST_INIT(entries);
	numEntries = 0;
	strValid = 0;
}

Error::~Error()
{
	Reset();
	ErrorEntry *e;
	while ((e = LIST_FIRST(ErrorEntry, list, entries))) {
		DestroyEntry(e);
	}
}

Error::ErrorDef *
Error::GetDef(ErrorCode code)
{
	for (int i = 0; i < (int)SIZEOFARRAY(errorDefs); i++) {
		if (errorDefs[i].code == code) {
			return &errorDefs[i];
		}
	}
	return 0;
}

Error::ErrorEntry *
Error::CreateEntry()
{
	ErrorEntry *e = NEW(ErrorEntry);

	return e;
}

void
Error::DestroyEntry(ErrorEntry *e)
{
	DELETE(e);
}

/* Either utilize entry from the cache or get new one */
Error::ErrorEntry *
Error::AddEntry()
{
	ErrorEntry *e;
	if ((e = LIST_FIRST(ErrorEntry, list, freeEntries))) {
		LIST_DELETE(list, e, freeEntries);
		assert(numFreeEntries);
		numFreeEntries--;
	} else {
		e = CreateEntry();
	}

	LIST_ADD_FIRST(list, e, entries);
	numEntries++;
	return e;
}

void
Error::FreeEntry(ErrorEntry *e)
{
	assert(numEntries);
	LIST_DELETE(list, e, entries);
	numEntries--;
	if (numFreeEntries >= MAX_FREE_ENTRIES) {
		DestroyEntry(e);
	} else {
		e->desc = (const char *)0;
		numFreeEntries++;
		LIST_ADD(list, e, freeEntries);
	}
}

int
Error::RaiseError(const char *function, int line, ErrorCode code,
		const char *fmt, ...)
{
	/* Do not do anything for interrupts servicing routines */
	CPU *cpu = CPU::GetCurrent();
	if (cpu->GetIntrNesting()) {
		return 0;
	}
	ErrorEntry *e = AddEntry();
	if (!e) {
		return -1;
	}
	e->function = function;
	e->line = line;
	e->code = code;
	if (fmt) {
		va_list args;
		va_start(args, fmt);
		e->desc.FormatV(fmt, args);
	}
	return 0;
}

int
Error::Reset()
{
	/* Remove all entries */
	ErrorEntry *e;
	while ((e = LIST_FIRST(ErrorEntry, list, entries))) {
		FreeEntry(e);
	}
	str = (const char *)0;
	strValid = 0;
	return 0;
}

Error::ErrorEntry *
Error::GetCurrent()
{
	return LIST_FIRST(ErrorEntry, list, entries);
}

Error::ErrorCode
Error::GetCode()
{
	ErrorEntry *e = GetCurrent();
	if (!e) {
		return E_SUCCESS;
	}
	return e->code;
}

int
Error::GetStr(char *buf, int bufLen)
{
	if (!strValid) {
		/* render current trace */
		if (numEntries) {
			ErrorEntry *e;
			LIST_FOREACH(ErrorEntry, list, e, entries) {
				ErrorDef *def = GetDef(e->code);
				assert(def);
				str.AppendFormat("%s@%d: %s (%d) - %s", e->function, e->line,
					def->designation, def->code, def->desc);
				if (e->desc.GetLength()) {
					str.AppendFormat(" (%s)", e->desc.GetBuffer());
				}
				str.Append('\n');
			}
		} else {
			str = "No error\n";
		}
		strValid = 1;
	}
	return str.Get(buf, bufLen);
}
