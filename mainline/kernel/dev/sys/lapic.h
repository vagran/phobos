/*
 * /kernel/dev/sys/lapic.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef LAPIC_H_
#define LAPIC_H_
#include <sys.h>
phbSource("$Id$");

class LAPIC : public Device {
private:
	enum {
		MEM_SIZE =		4096,
	};

	enum Regs {
		REG_ID =		0x020, /* Local APIC ID Register */
		REG_VERSION =	0x030, /* Local APIC Version Register */
		REG_TPR =		0x080, /* Task Priority Register */
		REG_APR =		0x090, /* Arbitration Priority Register */
		REG_PPR =		0x0a0, /* Processor Priority Register */
		REG_EOI =		0x0b0, /* EOI Register */
		REG_RRD =		0x0c0, /* Remote Read Register */
		REG_LDR =		0x0d0, /* Logical Destination Register */
		REG_DFR =		0x0e0, /* Destination Format Register */

		REG_SVR =		0x0f0, /* Spurious Interrupt Vector Register */
		REG_SVR_EN =	0x00000100, /* APIC Software Enable/Disable */
		REG_SVR_FPC =	0x00000200, /* Focus Processor Checking */

		REG_ISR =		0x100, /* In-Service Register */
		REG_TMR =		0x180, /* Trigger Mode Register */
		REG_IRR =		0x210, /* Interrupt Request Register */
		REG_ESR =		0x280, /* Error Status Register */
		REG_LVTCMCI =	0x2f0, /* LVT CMCI Registers */
		REG_ICR =		0x300, /* Interrupt Command Register */
		REG_LVTTMR =	0x320, /* LVT Timer Register */
		REG_LVTTSR =	0x330, /* LVT Thermal Sensor Register */
		REG_LVTPMC =	0x340, /* LVT Performance Monitoring Counters Register */
		REG_LINT0 =		0x350, /* LVT LINT0 Register */
		REG_LINT1 =		0x360, /* LVT LINT1 Register */
		REG_LVTER =		0x370, /* LVT Error Register */
		REG_TMRIC =		0x380, /* Initial Count Register (for Timer) */
		REG_TMRCC =		0x390, /* Current Count Register (for Timer) */
		REG_TMRDC =		0x3e0, /* Divide Configuration Register (for Timer) */
	};

	enum LVTEntryBits {
		LVTEB_VECTOR =		0x000000ff,

		LVTEB_DM =			0x00000700, /* Delivery Mode */
		LVTEB_DM_FIXED =	0x00000000,
		LVTEB_DM_SMI =		0x00000200,
		LVTEB_DM_NMI =		0x00000400,
		LVTEB_DM_EXTINT =	0x00000700,
		LVTEB_DM_INIT =		0x00000500,

		LVTEB_DS =			0x00001000, /* Delivery Status (0 - Idle, 1 - Send pending) */
		LVTEB_IIPP =		0x00002000, /* Interrupt Input Pin Polarity */
		LVTEB_RIRR =		0x00004000, /* Remote IRR */
		LVTEB_TM =			0x00008000, /* Trigger Mode (0 - Edge, 1 - Level) */
		LVTEB_MASK =		0x00010000, /* Mask (0 - Not masked, 1 - Masked) */
		LVTEB_TMRM =		0x00020000, /* Timer Mode (0 - One shot, 1 - Periodic) */
	};

	paddr_t memPhys;
	u8 *memMap;
	CPU *cpu;

	inline void WriteReg(u32 idx, u32 value);
	inline u32 ReadReg(u32 idx);
public:
	DeclareDevFactory();
	LAPIC(Type type, u32 unit, u32 classID);

	int SetCpu(CPU *cpu);
};

#endif /* LAPIC_H_ */
