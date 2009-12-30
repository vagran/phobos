/*
 * /kernel/kern/im.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#ifndef IM_H_
#define IM_H_
#include <sys.h>
phbSource("$Id$");

#include <dev/sys/pic.h>

/* Interrupts manager */

class IM {
public:
	enum {
		NUM_HWIRQ =		16,
		NUM_SWIRQ =		32,
		HWIRQ_BASE =	0x20, /* IVT base for hardware interrupts */
	};

	enum IsrStatus {
		IS_PROCESSED,
		IS_NOINTR, /* for shared lines, ISR must return this code if no interrupts pending */
		IS_ERROR,
		IS_PENDING,
	};

	enum IrqAllocFlags {
		AF_EXCLUSIVE =		0x1,
		AF_SPEC =			0x2, /* allocate at specified index */
	};

	typedef u32 irqmask_t;

	typedef IsrStatus (*ISR)(HANDLE h, void *arg);

private:
	enum IrqType {
		IT_HW,
		IT_SW,
	};

	enum IrqSlotFlags {
		SF_EXCLUSIVE =	0x1,
	};

	typedef struct {
		ListHead clients;
		u32 numClients;
		u32 flags;
	} IrqSlot;

	typedef struct {
		ListEntry list;
		IrqType type;
		int idx;
		ISR isr;
		void *arg;
		irqmask_t hwMask, swMask; /* interrupts to be disabled while calling this client */
	} IrqClient;

	IrqSlot hwIrq[NUM_HWIRQ];
	IrqSlot swIrq[NUM_SWIRQ];
	SpinLock slotLock;
	irqmask_t hwValid, swValid; /* locked by slotLock */
	irqmask_t hwDisabled;
	irqmask_t swPending, hwPending;
	SpinLock pendingLock; /* locked when clearing pending bit */
	/*
	 * Masks should not be modified while interrupt is active.
	 * The sequence is:
	 * 1) check it is not active, if it is then 1;
	 * 2) Acquire maskLock;
	 * 3) check still not active, if it is then release maskLock and 1;
	 * 4) modify mask;
	 * 5) release maskLock;
	 */
	u8 hwMasked[NUM_HWIRQ], swMasked[NUM_SWIRQ];
	irqmask_t hwActive, swActive;
	SpinLock maskLock;

	PIC *pic0, *pic1;

	IrqClient *Allocate(IrqType type, ISR isr, void *arg, u32 idx, u32 flags,
		irqmask_t hwMask, irqmask_t swMask);
	int Irq(IrqType type, u32 idx);
	IsrStatus ProcessInterrupt(IrqType type, u32 idx);
	int MaskIrq(IrqType type, u32 idx);
	int UnMaskIrq(IrqType type, u32 idx);
	static int HWIRQHandler(Frame *frame, void *arg);
	int GetPIC(u32 idx, PIC **ppic, u32 *pidx);
public:
	IM();

	HANDLE AllocateHwirq(ISR isr, void *arg = 0, u32 idx = 0, u32 flags = AF_EXCLUSIVE,
		irqmask_t hwMask = 0, irqmask_t swMask = 0);
	HANDLE AllocateSwirq(ISR isr, void *arg = 0, u32 idx = 0, u32 flags = AF_EXCLUSIVE,
		irqmask_t hwMask = 0, irqmask_t swMask = 0);
	int ReleaseIrq(HANDLE h);
	inline u32 GetIndex(HANDLE h) { assert(h); return ((IrqClient *)h)->idx; }
	static inline irqmask_t GetMask(u32 idx) { return (irqmask_t)1 << idx; }
	u32 DisableIntr(); /* return value used for RestoreIntr() */
	u32 RestoreIntr(u32 saved);
	int Hwirq(u32 idx);
	int Swirq(u32 idx);
	int HwEnable(u32 idx, int f = 1);
	int Poll();
};

extern IM *im;

#endif /* IM_H_ */
