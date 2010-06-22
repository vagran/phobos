/*
 * /phobos/kernel/sys/error.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef ERROR_H_
#define ERROR_H_
#include <sys.h>
phbSource("$Id$");

enum ErrorCode {
	E_SUCCESS = 0,			/* No error */
	E_FAULT = -1,			/* Unspecified error */
	E_NOMEM = -32767,		/* No memory */
	E_IO,					/* I/O operations error */
};

#endif /* ERROR_H_ */
