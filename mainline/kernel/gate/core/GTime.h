/*
 * /phobos/kernel/gate/GTime.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef GTIME_H_
#define GTIME_H_
#include <sys.h>
phbSource("$Id$");

DECLARE_GCLASS(GTime);

class GTime : public GateObject {
private:

public:
	GTime();
	virtual ~GTime();

	virtual u32 MS(u64 ms, u64 *pTicks = 0);
	virtual u32 S(u64 s, u64 *pTicks = 0);

	DECLARE_GCLASS_IMP(GTime);
};

#endif /* GTIME_H_ */
