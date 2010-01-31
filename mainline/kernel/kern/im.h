/*
 * /kernel/kern/im.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
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

	enum Priority {
		IP_DEFAULT =	-1,

		IP_TIMER,

		IP_RTC,
		IP_CLOCK,
		IP_MAX,
	};

	enum IsrStatus {
		IS_PROCESSED, /* All the work was done */
		IS_NOINTR, /* For shared lines, ISR should return this code if it able to detect that no interrupts pending */
		IS_ERROR, /* Processing error */
		IS_PENDING, /* Still have some work to do */
	};

	enum IrqAllocFlags {
		AF_EXCLUSIVE =		0x1,
		AF_SPEC =			0x2, /* allocate at specified index */
	};

	enum IrqType {
		IT_HW,
		IT_SW,
	};

	typedef u32 irqmask_t;

	typedef IsrStatus (*ISR)(Handle h, void *arg);

private:
	enum {
		MAX_POLL_NESTING =		16,
	};

	enum IrqSlotFlags {
		SF_EXCLUSIVE =	0x1,
	};

	struct _IrqClient;
	typedef struct _IrqClient IrqClient;

	typedef struct {
		ListEntry list; /* list sorted by line maximal priority values */
		ListHead clients;
		IrqClient *first; /* original first entry for restoring after rotation */
		u32 idx;
		u32 numClients;
		u32 flags;
		u32 lastRound; /* last round when it was serviced */
		int touched;
		/* The lowest and the highest priority of clients on this line */
		int minPri, maxPri;
	} IrqSlot;

	struct _IrqClient {
		ListEntry list; /* list sorted by client priority values */
		IrqType type;
		u32 idx;
		ISR isr;
		void *arg;
		u32 lastRound; /* last round when it was serviced */
		int serviceComplete;
		int priority;
		/*
		 * Interrupts to be disabled while calling this client,
		 * calculated dynamically according to the priority values.
		 */
		irqmask_t hwMask, swMask;
	};

	IrqSlot hwIrq[NUM_HWIRQ];
	IrqSlot swIrq[NUM_SWIRQ];
	ListHead hwSlots, swSlots;
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
	SpinLock activeLock, maskLock;
	u32 pollNesting; /* not true nesting since it accounts all processors */
	u32 roundIdx; /* incremented at each round */
	SpinLock selectLock, pollLock;
	u32 selectIdx;
	irqmask_t hwPLCache[IP_MAX + 1], swPLCache[IP_MAX + 1];

	PIC *pic0, *pic1;

	IrqClient *Allocate(IrqType type, ISR isr, void *arg, u32 idx, u32 flags, int priority);
	int Irq(IrqType type, u32 idx);
	static int HWIRQHandler(Frame *frame, void *arg);
	int GetPIC(u32 idx, PIC **ppic, u32 *pidx);
	IrqClient *SelectClient(IrqType type);
	IrqClient *SelectClient();
	IsrStatus CallClient(IrqClient *ic);
	int RecalculatePriorities(IrqType type);
	int RecalculatePriorities();
	irqmask_t RPGetMask(int priority, ListHead *slots);
	inline irqmask_t GetPLMask(int priority, IrqType type);
public:
	IM();

	Handle AllocateHwirq(ISR isr, void *arg = 0, u32 idx = 0,
		u32 flags = AF_EXCLUSIVE, int priority = IP_DEFAULT);
	Handle AllocateSwirq(ISR isr, void *arg = 0, u32 idx = 0,
		u32 flags = AF_EXCLUSIVE, int priority = IP_DEFAULT);
	int ReleaseIrq(Handle h);
	inline u32 GetIndex(Handle h) { assert(h); return ((IrqClient *)h)->idx; }
	static inline irqmask_t GetMask(u32 idx) { return (irqmask_t)1 << idx; }
	static u32 DisableIntr(); /* return value used for RestoreIntr() */
	static u32 EnableIntr(); /* return value used for RestoreIntr() */
	static u32 RestoreIntr(u32 saved);
	int Hwirq(u32 idx);
	int Swirq(u32 idx);
	int Irq(Handle h);
	int HwEnable(u32 idx, int f = 1);
	int HwAcknowledge(u32 idx);
	int Poll();
	int MaskIrq(IrqType type, u32 idx);
	int UnMaskIrq(IrqType type, u32 idx);
	u64 SetPL(int priority); /* set priority level, all interrupts with lower priority are masked */
	int RestorePL(u64 saved); /* argument is a value returned by SetPL() */
};

extern IM *im;

#endif /* IM_H_ */
