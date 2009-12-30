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
	hwPending = 0;
	swPending = 0;
	hwActive = 0;
	swActive = 0;
	memset(hwMasked, 0, sizeof(hwMasked));
	memset(swMasked, 0, sizeof(swMasked));
	pic0 = (PIC *)devMan.CreateDevice("pic", 0);
	if (!pic0) {
		panic("Failed to create Master PIC device");
	}
	pic1 = (PIC *)devMan.CreateDevice("pic", 1);
	if (!pic0) {
		panic("Failed to create Slave PIC device");
	}
	if (pic0->Initialize(HWIRQ_BASE)) {
		panic("pic0 initialization failed");
	}
	if (pic1->Initialize(HWIRQ_BASE + pic0->GetLinesCount())) {
		panic("pic1 initialization failed");
	}
	/* install our handler in IDT */
	for (int i = 0; i < NUM_HWIRQ; i++) {
		idt->RegisterHandler(HWIRQ_BASE + i, HWIRQHandler, this);
	}
}

int
IM::HWIRQHandler(Frame *frame, void *arg)
{
	((IM *)arg)->Hwirq(frame->vectorIdx - HWIRQ_BASE);
	PIC *pic;
	u32 picIdx;
	if (!((IM *)arg)->GetPIC(frame->vectorIdx - HWIRQ_BASE, &pic, &picIdx)) {
		pic->EOI(picIdx);
	}
	return 0;
}

int
IM::GetPIC(u32 idx, PIC **ppic, u32 *pidx)
{
	if (idx >= NUM_HWIRQ) {
		return -1;
	}
	if (ppic) {
		*ppic = idx >= 8 ? pic1 : pic0;
	}
	if (pidx) {
		*pidx = idx & 0x7;
	}
	return 0;
}

int
IM::HwEnable(u32 idx, int f)
{
	if (idx >= NUM_HWIRQ) {
		klog(KLOG_ERROR, "Hardware IRQ index out of range (%lu)", idx);
		return -1;
	}
	irqmask_t mask = GetMask(idx);
	if (!(mask & hwValid)) {
		klog(KLOG_ERROR, "Attempted to enable not allocated hardware interrupt");
		return -1;
	}
	PIC *pic;
	u32 picIdx;
	GetPIC(idx, &pic, &picIdx);
	if (f) {
		pic->EnableInterrupt(picIdx);
	} else {
		pic->DisableInterrupt(picIdx);
	}
	return 0;
}

int
IM::Hwirq(u32 idx)
{
	return Irq(IT_HW, idx);
}

int
IM::Swirq(u32 idx)
{
	return Irq(IT_SW, idx);
}

int
IM::Irq(IrqType type, u32 idx)
{
	irqmask_t mask, *validMask, *activeMask;
	u8 *masked;
	u32 numLines;
	if (type == IT_HW) {
		assert(idx <= NUM_HWIRQ);
		validMask = &hwValid;
		masked = &hwMasked[idx];
		activeMask = &hwActive;
		numLines = NUM_HWIRQ;
	} else if (type == IT_SW) {
		assert(idx <= NUM_SWIRQ);
		validMask = &swValid;
		masked = &swMasked[idx];
		activeMask = &swActive;
		numLines = NUM_SWIRQ;
	} else {
		panic("Invalid interrupt type (%d)", type);
	}
	if (idx >= numLines) {
		panic("Interrupt request with invalid index (%lu/%lu)", idx, numLines);
	}
	mask = GetMask(idx);
	if (!(mask & *validMask)) {
		panic("Interrupt request on invalid line (line %lu, type %d)", idx, type);
	}
	/* make it pending */
	if (type == IT_SW) {
		AtomicOp::Or(&swPending, mask);
	} else if (type == IT_HW) {
		if (mask & hwDisabled) {
			klog(KLOG_WARNING, "Hardware interrupt request on disabled line (%lu)", idx);
			return -1;
		}
		AtomicOp::Or(&hwPending, mask);
	}
	/* try to handle */
	if (!*masked) {
		ProcessInterrupt(type, idx);
	}
	return 0;
}

int
IM::Poll()
{
	for (u32 idx = 0; idx < NUM_HWIRQ; idx++) {
		irqmask_t mask = GetMask(idx);
		if (!(mask & hwValid)) {
			continue;
		}
		if (!(mask & hwPending)) {
			continue;
		}
		if (!hwMasked[idx]) {
			ProcessInterrupt(IT_HW, idx);
		}
	}
	for (u32 idx = 0; idx < NUM_SWIRQ; idx++) {
		irqmask_t mask = GetMask(idx);
		if (!(mask & swValid)) {
			continue;
		}
		if (!(mask & swPending)) {
			continue;
		}
		if (!swMasked[idx]) {
			ProcessInterrupt(IT_SW, idx);
		}
	}
	return 0;
}

int
IM::MaskIrq(IrqType type, u32 idx)
{
	irqmask_t *activeMask;
	u8 *masked;

	irqmask_t mask = GetMask(idx);
	if (type == IT_HW) {
		assert(mask & hwValid);
		assert(idx <= NUM_HWIRQ);
		masked = &hwMasked[idx];
		activeMask = &hwActive;
	} else if (type == IT_SW) {
		assert(mask & swValid);
		assert(idx <= NUM_SWIRQ);
		masked = &swMasked[idx];
		activeMask = &swActive;
	} else {
		panic("Invalid IRQ type %d", type);
	}
	while (1) {
		if (mask & *activeMask) {
			/* some other CPU is processing interrupt, wait for completion */
			/* May be we should switch context here? */
			continue;
		}
		maskLock.Lock();
		if (mask & *activeMask) {
			maskLock.Unlock();
			continue;
		}
		/* locked by maskLock so doesn't need to be atomic */
		*masked++;
		maskLock.Unlock();
		break;
	}
	return 0;
}

int
IM::UnMaskIrq(IrqType type, u32 idx)
{
	irqmask_t *pendingMask;
	u8 *masked;

	irqmask_t mask = GetMask(idx);
	/* Process pending interrupt if present */
	if (type == IT_HW) {
		assert(idx <= NUM_HWIRQ);
		assert(mask & hwValid);
		masked = &hwMasked[idx];
		pendingMask = &hwPending;
	} else if (type == IT_SW) {
		assert(idx <= NUM_SWIRQ);
		assert(mask & swValid);
		masked = &swMasked[idx];
		pendingMask = &swPending;
	} else {
		panic("Invalid IRQ type %d", type);
	}

	maskLock.Lock();
	assert(*masked);
	u8 isMasked = --(*masked);
	maskLock.Unlock();
	if (!isMasked && (mask & *pendingMask)) {
		ProcessInterrupt(type, idx);
	}
	return 0;
}

IM::IsrStatus
IM::ProcessInterrupt(IrqType type, u32 idx)
{
	IrqSlot *is;
	irqmask_t *pendingMask, *activeMask;
	u8 *masked;

	irqmask_t mask = GetMask(idx);
	if (type == IT_HW) {
		assert(idx <= NUM_HWIRQ);
		assert(mask & hwValid);
		is = &hwIrq[idx];
		pendingMask = &hwPending;
		masked = &hwMasked[idx];
		activeMask = &hwActive;
	} else if (type == IT_SW) {
		assert(idx <= NUM_SWIRQ);
		assert(mask & swValid);
		is = &swIrq[idx];
		pendingMask = &swPending;
		masked = &swMasked[idx];
		activeMask = &swActive;
	} else {
		panic("Invalid IRQ type %d", type);
	}
	IrqClient *ic;
	IsrStatus status = IS_ERROR;

	AtomicOp::Or(activeMask, mask);
	maskLock.Lock();
	irqmask_t isMasked = *masked;
	maskLock.Unlock();
	if (isMasked) {
		return IS_PENDING;
	}

	pendingLock.Lock();
	if (!(*pendingMask & mask)) {
		pendingLock.Unlock();
		AtomicOp::And(activeMask, ~mask);
		return IS_NOINTR;
	}
	/* should still be atomic since bit setting is not protected by lock */
	AtomicOp::And(pendingMask, ~mask);
	pendingLock.Unlock();
	slotLock.Lock(); /* nobody should modify the list while we are processing it */
	if (!is->numClients) {
		panic("IRQ processing on empty slot (type %d, line %lu)", type, idx);
	}
	assert(!LIST_ISEMPTY(is->clients));
	irqmask_t hwMask = 0, swMask = 0;
	LIST_FOREACH(IrqClient, list, ic, is->clients) {
		/* disable requested interrupts */
		for (u32 i = 0; i < NUM_HWIRQ; i++) {
			if (type == IT_HW && i == idx) {
				continue;
			}
			irqmask_t curMask = GetMask(i);
			if (ic->hwMask & curMask) {
				if (!(hwMask & curMask)) {
					hwMask |= curMask;
					MaskIrq(IT_HW, i);
				}
			} else {
				if (hwMask & curMask) {
					hwMask &= ~curMask;
					UnMaskIrq(IT_HW, i);
				}
			}
		}
		for (u32 i = 0; i < NUM_SWIRQ; i++) {
			if (type == IT_SW && i == idx) {
				continue;
			}
			irqmask_t curMask = GetMask(i);
			if (ic->swMask & curMask) {
				if (!(swMask & curMask)) {
					swMask |= curMask;
					MaskIrq(IT_SW, i);
				}
			} else {
				if (swMask & curMask) {
					swMask &= ~curMask;
					UnMaskIrq(IT_SW, i);
				}
			}
		}
		status = ic->isr((HANDLE)ic, ic->arg);
		if (status != IS_NOINTR) {
			break;
		}
	}
	/* unmask masked interrupts */
	for (u32 i = 0; i < NUM_HWIRQ; i++) {
		if (type == IT_HW && i == idx) {
			continue;
		}
		irqmask_t curMask = GetMask(i);
		if (hwMask & curMask) {
			UnMaskIrq(IT_HW, i);
		}
	}
	for (u32 i = 0; i < NUM_SWIRQ; i++) {
		if (type == IT_SW && i == idx) {
			continue;
		}
		irqmask_t curMask = GetMask(i);
		if (swMask & curMask) {
			UnMaskIrq(IT_SW, i);
		}
	}
	slotLock.Unlock();
	AtomicOp::And(activeMask, ~mask);
	return status;
}

HANDLE
IM::AllocateHwirq(ISR isr, void *arg, u32 idx, u32 flags,
	irqmask_t hwMask, irqmask_t swMask)
{
	return (HANDLE)Allocate(IT_HW, isr, arg, idx, flags, hwMask, swMask);
}

HANDLE
IM::AllocateSwirq(ISR isr, void *arg, u32 idx, u32 flags,
	irqmask_t hwMask, irqmask_t swMask)
{
	return (HANDLE)Allocate(IT_SW, isr, arg, idx, flags, hwMask, swMask);
}

IM::IrqClient *
IM::Allocate(IrqType type, ISR isr, void *arg, u32 idx, u32 flags,
	irqmask_t hwMask, irqmask_t swMask)
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
	ic->hwMask = hwMask;
	ic->swMask = swMask;
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
