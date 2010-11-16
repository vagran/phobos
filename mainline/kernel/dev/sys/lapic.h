/*
 * /kernel/dev/sys/lapic.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#ifndef LAPIC_H_
#define LAPIC_H_
#include <sys.h>
phbSource("$Id$");

class LAPIC : public Device {
public:
	typedef enum {
		DM_FIXED =		0x0,
		DM_LOWPRI =		0x1,
		DM_SMI =		0x2,
		DM_NMI =		0x4,
		DM_INIT =		0x5,
		DM_STARTUP =	0x6,
	} DeliveryMode;

	typedef enum {
		DST_SPECIFIC =	0x0,
		DST_SELF =		0x1,
		DST_ALL =		0x2,
		DST_OTHERS =	0x3,
	} Destination;

	typedef enum {
		DSTMODE_PHYSICAL =	0x0,
		DSTMODE_LOGICAL =	0x1,
	} DestMode;

	typedef enum {
		LVL_DEASSERT =		0x0,
		LVL_ASSERT =		0x1,
	} Level;

	typedef enum {
		TM_EDGE =			0x0,
		TM_LEVEL =			0x1,
	} TriggerMode;

	typedef enum {
		IPI_VECTOR =				0x80,
		IPI_DEACTIVATE_THREAD =		0,

		IPI_MAX
	} IPIType;
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
		REG_ICR_DM_MASK =		0x00000700, /* Delivery Mode */
		REG_ICR_DM_FIXED =		0x00000000,
		REG_ICR_DM_LOWPRI =		0x00000100,
		REG_ICR_DM_SMI =		0x00000200,
		REG_ICR_DM_NMI =		0x00000400,
		REG_ICR_DM_INIT =		0x00000500,
		REG_ICR_DM_STARTUP =	0x00000600,
		REG_ICR_DESTMODE =		0x00000800, /* Destination Mode (0 - Physical, 1 - Logical) */
		REG_ICR_DS =			0x00001000, /* Delivery Status (0 - Idle, 1 - Send pending) */
		REG_ICR_LEVEL =			0x00004000, /* Level (0 - De-assert, 1 - Assert) */
		REG_ICR_TM =			0x00008000, /* Trigger Mode (0 - Edge, 1 - Level) */
		REG_ICR_DSH_MASK =		0x000c0000, /* Destination Shorthand */
		REG_ICR_DSH_SPEC =		0x00000000,
		REG_ICR_DSH_SELF =		0x00040000,
		REG_ICR_DSH_ALL =		0x00080000,
		REG_ICR_DSH_OTHERS =	0x000c0000,

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
	u32 ID;

	inline void WriteReg(u32 idx, u32 value);
	inline u32 ReadReg(u32 idx);
public:
	DeclareDevFactory();
	LAPIC(Type type, u32 unit, u32 classID);

	inline u32 GetID() { return ID; }
	int SetCpu(CPU *cpu);
	int SendIPI(DeliveryMode dm, Destination dst, u32 wait = 0xffffffff, u32 vector = 0, u32 ID = 0,
		int destMode = DSTMODE_PHYSICAL, int level = LVL_ASSERT, int triggerMode = TM_EDGE);
	int WaitIPI(u32 delay = 0xffffffff); /* ret zero if IPI completed */
};

#endif /* LAPIC_H_ */
