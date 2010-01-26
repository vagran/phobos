/*
 * /kernel/kern/tm.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef TM_H_
#define TM_H_
#include <sys.h>
phbSource("$Id$");

#include <dev/sys/pit.h>

/* Time Manager */

class TM {
private:
	PIT *pit;

public:
	TM();


};

extern TM *tm;

#endif /* TM_H_ */
