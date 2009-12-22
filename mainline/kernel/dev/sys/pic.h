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

#include <dev/sys/IntController.h>

class PIC : public IntController {
private:
	enum Ports {
		PORT_M_ICW =	0x20,
		PORT_M_OCW =	0x21,
		PORT_M_ELCR =	0x4d0,
		PORT_S_ICW =	0xa0,
		PORT_S_OCW =	0xa1,
		PORT_S_ELCR =	0x4d1,
	};

	enum Regs {
		ICW1_IC4 =			0x1, /* ICW4 write required */
		ICW1_SNGL =			0x2, /* single/cascade mode */
		ICW1_ADI =			0x4, /* must be set */
		ICW1_LTIM =			0x8, /* Edge/Level Bank Select, must be cleared */
		ICW1_ICWOCWSEL =	0x10, /* ICW/OCW select */

		ICW3_M =			0x4, /* master controller initialization */
		ICW3_S =			0x2, /* slave controller initialization */

		ICW4_MM =			0x1, /* Microprocessor Mode, must be set */
		ICW4_AEOI =			0x2, /* Automatic End of Interrupt */
		ICW4_MSBM =			0x4, /* Master/Slave in Buffered Mode, must be cleared */
		ICW4_BUF =			0x8, /* Buffere Mode, must be cleared */
		ICW4_SFNM =			0x10, /* Special Fully Nested Mode */

		oCW2_LEVEL_MASK =	0x7,
		OCW2_EOI =			0x20, /* End Of Interrupt */
		OCW2_SL =			0x40, /* Select Level */
		OCW2_R =			0x80, /* Rotate */

		OCW3_RD_IRR =		0x0, /* read IRQ register */
		OCW3_RD_ISR =		0x1, /* read In-Service register */
		OCW3_RD_SEL =		0x2, /* select read target */
		OCW3_POLL =			0x4, /* Poll Mode Command */
		OCW3_SELECT =		0x8, /* OCW3 select */
		OCW3_ESMM =			0x20, /* Enable Special Mask Mode */
		OCW3_SMM =			0x40, /* Special Mask Mode */
	};

	enum {
		NUM_LINES =			8,
	};

	u16 portICW, portOCW, portELCR;

public:
	PIC(Type type, u32 unit, u32 classID);
	DeclareDevFactory();

	/* IntController implementation */
	virtual u32 GetLinesCount();
	virtual int Initialize(u32 ivtBase);
	virtual int EnableInterrupt(u32 idx);
	virtual int DisableInterrupt(u32 idx);
};

#endif /* PIC_H_ */
