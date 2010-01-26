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

DefineDevFactory(PIC);

RegDevClass(PIC, "pic", Device::T_SPECIAL, "Programmable Interrupts Controller (8259)");

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
	isInitialized = 0;
	devState = S_UP;
}

u32
PIC::GetLinesCount()
{
	return NUM_LINES;
}

int
PIC::Initialize(u32 ivtBase)
{
	if (ivtBase & 0x7) {
		klog(KLOG_ERROR, "Base vector for PIC must be 8-aligned (passed %lu)", ivtBase);
		return -1;
	}
	if (ivtBase & ~0xff) {
		klog(KLOG_ERROR, "Base vector for PIC exceeded the limit (passed %lu)", ivtBase);
	}
	outb(portICW, ICW1_ICWOCWSEL | ICW1_IC4);
	outb(portOCW, ivtBase);
	outb(portOCW, devUnit ? ICW3_S : ICW3_M);
	outb(portOCW, ICW4_MM);
	/*
	 * Initialization sequence complete, disable all interrupts.
	 * Do not disable cascaded slave line in master controller.
	 */
	intMask = devUnit ? 0xff : 0xff & ~GetMask(SLAVE_LINE_IDX);
	outb(portOCW, intMask);
	isInitialized = 1;
	return 0;
}

int
PIC::EnableInterrupt(u32 idx)
{
	assert(isInitialized);
	if (idx > NUM_LINES) {
		klog(KLOG_ERROR, "Line index is out of range (%lu)", idx);
		return -1;
	}
	irqmask_t mask = GetMask(idx);
	if (intMask & mask) {
		intMask &= ~mask;
		outb(portOCW, intMask);
	}
	return 0;
}

int
PIC::DisableInterrupt(u32 idx)
{
	assert(isInitialized);
	if (idx > NUM_LINES) {
		klog(KLOG_ERROR, "Line index is out of range (%lu)", idx);
		return -1;
	}
	irqmask_t mask = GetMask(idx);
	if (!(intMask & mask)) {
		intMask |= mask;
		outb(portOCW, intMask);
	}
	return 0;
}

int
PIC::EOI(u32 idx)
{
	assert(isInitialized);
	if (idx != DEF_IDX && idx > NUM_LINES) {
		klog(KLOG_ERROR, "Line index is out of range (%lu)", idx);
		return -1;
	}
	u8 cmd = OCW2_EOI;
	if (idx != DEF_IDX) {
		cmd |= OCW2_SL | (u8)idx;
	}
	outb(portICW, cmd);
	return 0;
}
