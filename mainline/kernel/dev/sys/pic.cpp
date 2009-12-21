/*
 * /kernel/dev/sys/pic.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <dev/sys/pic.h>

PIC::PIC(Type type, u32 unit, u32 classID) : Device(type, unit, classID)
{
	switch (unit) {
	case 0:
		portICW = PORT_M_ICW;
		portOCW = PORT_M_OCW;
		portELCR = PORT_M_ELCR;
		break;
	case 1:
		portICW = PORT_S_ICW;
		portOCW = PORT_S_OCW;
		portELCR = PORT_S_ELCR;
		break;
	default:
		return;
	}

	devState = S_UP;
}

DefineDevFactory(PIC);

RegDevClass(PIC, "pic", Device::T_SPECIAL, "Programmable Interrupts Controller (8259)");
