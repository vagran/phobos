/*
 * /kernel/dev/sys/IntController.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef INTCONTROLLER_H_
#define INTCONTROLLER_H_
#include <sys.h>
phbSource("$Id$");

/* Base class which declares interface for PIC and I/O APIC */

class IntController : public Device {
public:
	enum {
		DEF_IDX =		0xffffffff,
	};

	IntController(Type type, u32 unit, u32 classID) :
		Device(type, unit, classID) {}

	virtual u32 GetLinesCount() = 0;
	/* all interrupts must be disabled after successful initialization */
	virtual int Initialize(u32 ivtBase) = 0;
	virtual int EnableInterrupt(u32 idx) = 0;
	virtual int DisableInterrupt(u32 idx) = 0;
	virtual int EOI(u32 idx = DEF_IDX) = 0;
};

#endif /* INTCONTROLLER_H_ */
