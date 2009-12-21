/*
 * /kernel/dev/sys/pic.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef PIC_H_
#define PIC_H_
#include <sys.h>
phbSource("$Id$");

class PIC : public Device {
private:
	enum {
		PORT_M_ICW =	0x20,
		PORT_M_OCW =	0x21,
		PORT_M_ELCR =	0x4d0,
		PORT_S_ICW =	0xa0,
		PORT_S_OCW =	0xa1,
		PORT_S_ELCR =	0x4d1,
	};

	u16 portICW, portOCW, portELCR;

public:
	PIC(Type type, u32 unit, u32 classID);
	DeclareDevFactory();

};

#endif /* PIC_H_ */
