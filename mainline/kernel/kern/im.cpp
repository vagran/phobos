/*
 * /kernel/kern/im.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");


IM *im;

IM::IM()
{
	memset(hwIrq, 0, sizeof(hwIrq));
	for (int i = 0; i < NUM_HWIRQ; i++) {
		LIST_INIT(hwIrq[i].clients);
	}
	memset(swIrq, 0, sizeof(swIrq));
	for (int i = 0; i < NUM_SWIRQ; i++) {
		LIST_INIT(swIrq[i].clients);
	}
	hwValid = 0;
	swValid = 0;
}

HANDLE
IM::AllocateHwirq(ISR isr, void *arg, u32 idx, u32 flags)
{
	return (HANDLE)Allocate(IT_HW, isr, arg, idx, flags);
}

HANDLE
IM::AllocateSwirq(ISR isr, void *arg, u32 idx, u32 flags)
{
	return (HANDLE)Allocate(IT_SW, isr, arg, idx, flags);
}

IM::IrqClient *
IM::Allocate(IrqType type, ISR isr, void *arg, u32 idx, u32 flags)
{
	IrqSlot *isa;
	u32 numSlots;
	irqmask_t *maskValid;

	if (type == IT_HW) {
		isa = hwIrq;
		numSlots = NUM_HWIRQ;
		maskValid = &hwValid;
	} else if (type == IT_SW) {
		isa = swIrq;
		numSlots = NUM_SWIRQ;
		maskValid = &swValid;
	} else {
		panic("IM::Allocate: invalid IRQ type: %d", type);
	}

	u32 x = DisableIntr();
	slotLock.Lock();
	IrqSlot *is;
	if (flags & AF_SPEC) {
		if (idx >= numSlots) {
			slotLock.Unlock();
			RestoreIntr(x);
			klog(KLOG_ERROR, "Specified IRQ index is out of range (type %d: %lu/%lu)",
				type, idx, numSlots);
			return 0;
		}
		is = &isa[idx];
		if (is->numClients && (flags & AF_EXCLUSIVE)) {
			slotLock.Unlock();
			RestoreIntr(x);
			klog(KLOG_ERROR, "Attempted to allocate exclusive IRQ on busy slot (type %d: %lu)",
				type, idx);
			return 0;
		}
		if (is->numClients && (is->flags & SF_EXCLUSIVE)) {
			slotLock.Unlock();
			RestoreIntr(x);
			klog(KLOG_ERROR, "Attempted to share exclusive IRQ (type %d: %lu)",
				type, idx);
			return 0;
		}
	} else {
		/* find the most suitable slot */
		is = 0;
		for (u32 i = 0; i < numSlots; i++) {
			IrqSlot *p = &isa[i];
			if (p->numClients && (p->flags & SF_EXCLUSIVE)) {
				continue;
			}
			if ((flags & AF_EXCLUSIVE) && p->numClients) {
				continue;
			}
			if (!is || p->numClients < is->numClients) {
				is = p;
				idx = i;
				if (!is->numClients) {
					break;
				}
			}
		}
	}
	if (!is) {
		slotLock.Unlock();
		RestoreIntr(x);
		klog(KLOG_ERROR, "No suitable slot found for IRQ (type %d)", type);
		return 0;
	}
	IrqClient *ic = NEW(IrqClient);
	if (!ic) {
		slotLock.Unlock();
		RestoreIntr(x);
		klog(KLOG_ERROR, "IrqClient allocation failed (type %d)", type);
		return 0;
	}
	ic->idx = idx;
	ic->type = type;
	ic->isr = isr;
	ic->arg = arg;
	LIST_ADD(list, ic, is->clients);
	is->numClients++;
	if (flags & AF_EXCLUSIVE) {
		is->flags |= SF_EXCLUSIVE;
	} else {
		is->flags &= ~SF_EXCLUSIVE;
	}
	*maskValid |= GetMask(idx);
	slotLock.Unlock();
	RestoreIntr(x);
	return ic;
}

int
IM::ReleaseIrq(HANDLE h)
{
	if (!h) {
		return -1;
	}
	IrqClient *ic = (IrqClient *)h;
	IrqSlot *is;
	irqmask_t *maskValid;
	if (ic->type == IT_HW) {
		assert(ic->idx < NUM_HWIRQ);
		is = &hwIrq[ic->idx];
		maskValid = &hwValid;
	} else if (ic->type == IT_SW) {
		assert(ic->idx < NUM_SWIRQ);
		is = &swIrq[ic->idx];
		maskValid = &swValid;
	} else {
		panic("Invalid IrqClient type (%d)", ic->type);
	}
	u32 x = DisableIntr();
	slotLock.Lock();
	LIST_DELETE(list, ic, is->clients);
	if (!--is->numClients) {
		*maskValid &= ~GetMask(ic->idx);
	}
	slotLock.Unlock();
	RestoreIntr(x);
	DELETE(ic);
	return 0;
}

u32
IM::DisableIntr()
{
	u32 rc = GetEflags();
	cli();
	return rc;
}

u32
IM::RestoreIntr(u32 saved)
{
	if (saved & EFLAGS_IF) {
		sti();
	} else {
		cli();
	}
	return saved;
}
