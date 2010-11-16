/*
 * /phobos/bin/rt_linker/rt_linker.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#ifndef RT_LINKER_H_
#define RT_LINKER_H_
#ifndef SYS_H_
#include <sys.h>
#endif /* SYS_H_ */
phbSource("$Id$");

#include <libelf.h>

class RTLinker : public Object {
private:
	CString target;

public:
	RTLinker();
	~RTLinker();

	int Link();
};

#endif /* RT_LINKER_H_ */
