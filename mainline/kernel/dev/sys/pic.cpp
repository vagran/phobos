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

PIC::PIC(Type type, u32 unit, u32 classID) : IntController(type, unit, classID)
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

u32
PIC::GetLinesCount()
{
	return NUM_LINES;
}

int
PIC::Initialize(u32 ivtBase)
{

	return 0;
}

int
PIC::EnableInterrupt(u32 idx)
{
	/* notimpl */
	return 0;
}

int
PIC::DisableInterrupt(u32 idx)
{
	/* notimpl */
	return 0;
}
