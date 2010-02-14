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
	LIST_INIT(hwSlots);
	for (int i = 0; i < NUM_HWIRQ; i++) {
		LIST_INIT(hwIrq[i].clients);
		hwIrq[i].idx = i;
	}

	memset(swIrq, 0, sizeof(swIrq));
	LIST_INIT(swSlots);
	for (int i = 0; i < NUM_SWIRQ; i++) {
		LIST_INIT(swIrq[i].clients);
		swIrq[i].idx = i;
	}

	hwValid = 0;
	swValid = 0;
	hwPending = 0;
	swPending = 0;
	hwActive = 0;
	swActive = 0;
	hwDisabled = ~0;
	pollNesting = 0;
	roundIdx = 0;
	selectIdx = 0;
	memset(hwMasked, 0, sizeof(hwMasked));
	memset(swMasked, 0, sizeof(swMasked));
	memset(hwPLCache, 0, sizeof(hwPLCache));
	memset(swPLCache, 0, sizeof(swPLCache));
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
		hwDisabled &= ~mask;
		pic->EnableInterrupt(picIdx);
	} else {
		pic->DisableInterrupt(picIdx);
		hwDisabled |= mask;
	}
	return 0;
}

int
IM::Hwirq(u32 idx, int stiEnabled)
{
	return Irq(IT_HW, idx, stiEnabled);
}

int
IM::Swirq(u32 idx)
{
	return Irq(IT_SW, idx, GetEflags() & EFLAGS_IF);
}

int
IM::Irq(Handle h)
{
	IrqClient *ic = (IrqClient *)h;
	if (ic->type == IT_HW) {
		return Hwirq(ic->idx);
	} else if (ic->type == IT_SW) {
		return Swirq(ic->idx);
	}
	panic("Invalid interrupt type (%d)", ic->type);
}

int
IM::Irq(IrqType type, u32 idx, int stiEnabled)
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

	/* acknowledge hardware interrupt reception */
	if (type == IT_HW) {
		HwAcknowledge(idx);
	}

	/* try to handle */
	if (!*masked && !(mask & *activeMask) && pollNesting < MAX_POLL_NESTING) {
		Poll(stiEnabled);
	}
	return 0;
}

int
IM::HwAcknowledge(u32 idx)
{
	PIC *pic;
	u32 picIdx;
	if (!GetPIC(idx, &pic, &picIdx)) {
		if (pic == pic1) {
			pic0->EOI(PIC::IRQ_SLAVEPIC);
		}
		return pic->EOI(picIdx);
	}
	return -1;
}

IM::IrqClient *
IM::SelectClient(IrqType type)
{
	u32 numLines;
	irqmask_t *validMask, *activeMask, *pendingMask;
	u8 *masked;
	IrqSlot *slots;
	ListHead *slotsHead;

	if (type == IT_HW) {
		validMask = &hwValid;
		masked = hwMasked;
		activeMask = &hwActive;
		pendingMask = &hwPending;
		numLines = NUM_HWIRQ;
		slots = hwIrq;
		slotsHead = &hwSlots;
	} else if (type == IT_SW) {
		validMask = &swValid;
		masked = swMasked;
		activeMask = &swActive;
		pendingMask = &swPending;
		numLines = NUM_SWIRQ;
		slots = swIrq;
		slotsHead = &swSlots;
	} else {
		panic("Invalid interrupt type (%d)", type);
	}

	if (!(*pendingMask & (~*activeMask) & *validMask)) {
		return 0;
	}

	slotLock.Lock();
	while (1) {
		IrqSlot *is;
		int derefered = 0;
		LIST_FOREACH(IrqSlot, list, is, *slotsHead) {
			irqmask_t mask = GetMask(is->idx);
			if (!(mask & *validMask)) {
				continue;
			}
			if (!(mask & *pendingMask)) {
				continue;
			}
			if (mask & *activeMask) {
				continue;
			}
			if (masked[is->idx]) {
				continue;
			}
			if (is->lastRound != roundIdx) {
				is->lastRound = roundIdx;
				is->touched = 0;
				LIST_ROTATE(list, is->first, is->clients);
			} else {
				if (is->touched) {
					derefered = 1;
					continue;
				}
			}
			assert(is->numClients && !LIST_ISEMPTY(is->clients));

			while (1) {
				IrqClient *ic;
				LIST_FOREACH(IrqClient, list, ic, is->clients) {
					if (ic->lastRound != roundIdx) {
						ic->serviceComplete = 0;
						ic->lastRound = roundIdx;
					} else {
						if (ic->serviceComplete) {
							continue;
						}
					}
					LIST_ROTATE(list, LIST_NEXT(IrqClient, list, ic), is->clients);
					is->touched = 1;
					slotLock.Unlock();
					return ic;
				}
				/*
				 * We have pending interrupt on this line while all clients have processed it.
				 * Probably new interrupt occurred and we should restart servicing cycle for this slot.
				 */
				LIST_FOREACH(IrqClient, list, ic, is->clients) {
					ic->serviceComplete = 0;
				}
			}
			/* NOT REACHED */
		}
		if (!derefered) {
			break;
		}
		/*
		 * We have completed one iteration and have lines which still require servicing.
		 * Reset 'touched' status and proceed to the next iteration.
		 */
		LIST_FOREACH(IrqSlot, list, is, *slotsHead) {
			is->touched = 0;
		}
	}
	slotLock.Unlock();
	return 0;
}

IM::IrqClient *
IM::SelectClient()
{
	/* This method must be atomic */
	selectLock.Lock();
	selectIdx++; /* for round robin between hardware and software queues */
	IrqClient *ic;
	if (selectIdx & 1) {
		ic = IM::SelectClient(IT_HW);
	} else {
		ic = IM::SelectClient(IT_SW);
	}
	if (!ic) {
		if (selectIdx & 1) {
			ic = IM::SelectClient(IT_SW);
		} else {
			ic = IM::SelectClient(IT_HW);
		}
	}
	selectLock.Unlock();
	return ic;
}

/* must be called with interrupts disabled */
IM::IsrStatus
IM::CallClient(IrqClient *ic, int stiEnabled)
{
	irqmask_t *pendingMask, *activeMask;
	u8 *masked;
	IrqType type = ic->type;
	u32 idx = ic->idx;

	assert(!(GetEflags() & EFLAGS_IF));
	irqmask_t mask = GetMask(idx);
	if (type == IT_HW) {
		assert(idx <= NUM_HWIRQ);
		assert(mask & hwValid);
		pendingMask = &hwPending;
		masked = &hwMasked[idx];
		activeMask = &hwActive;
	} else if (type == IT_SW) {
		assert(idx <= NUM_SWIRQ);
		assert(mask & swValid);
		pendingMask = &swPending;
		masked = &swMasked[idx];
		activeMask = &swActive;
	} else {
		panic("Invalid IRQ type %d", type);
	}
	IsrStatus status = IS_ERROR;

	activeLock.Lock();
	if (*activeMask & mask) {
		/* already handled by another processor */
		activeLock.Unlock();
		return IS_NOINTR;
	}
	AtomicOp::Or(activeMask, mask);
	activeLock.Unlock();

	if (*masked) {
		AtomicOp::And(activeMask, ~mask);
		return IS_PENDING;
	}

	pendingLock.Lock();
	if (!(*pendingMask & mask)) {
		pendingLock.Unlock();
		AtomicOp::And(activeMask, ~mask);
		return IS_NOINTR;
	}
	pendingLock.Unlock();

	CPU *cpu = CPU::GetCurrent();
	if (cpu) {
		cpu->NestInterrupt();
	}

	/* disable requested interrupts */
	if (ic->hwMask) {
		for (u32 i = 0; i < NUM_HWIRQ; i++) {
			if (type == IT_HW && i == idx) {
				continue;
			}
			irqmask_t curMask = GetMask(i);
			if (ic->hwMask & curMask) {
				MaskIrq(IT_HW, i);
			}
		}
	}
	if (ic->swMask) {
		for (u32 i = 0; i < NUM_SWIRQ; i++) {
			if (type == IT_SW && i == idx) {
				continue;
			}
			irqmask_t curMask = GetMask(i);
			if (ic->swMask & curMask) {
				MaskIrq(IT_SW, i);
			}
		}
	}

	AtomicOp::And(pendingMask, ~mask);
	if (stiEnabled) {
		sti();
	}
	status = ic->isr((Handle)ic, ic->arg);
	if (stiEnabled) {
		cli();
	}
	if (status == IS_PENDING || status == IS_NOINTR) {
		AtomicOp::Or(pendingMask, mask);
	}
	if (status != IS_PENDING) {
		ic->serviceComplete = 1;
	}

	/* unmask masked interrupts */
	if (ic->swMask) {
		for (u32 i = 0; i < NUM_SWIRQ; i++) {
			if (type == IT_SW && i == idx) {
				continue;
			}
			irqmask_t curMask = GetMask(i);
			if (ic->swMask & curMask) {
				UnMaskIrq(IT_SW, i, stiEnabled);
			}
		}
	}
	if (ic->hwMask) {
		for (u32 i = 0; i < NUM_HWIRQ; i++) {
			if (type == IT_HW && i == idx) {
				continue;
			}
			irqmask_t curMask = GetMask(i);
			if (ic->hwMask & curMask) {
				UnMaskIrq(IT_HW, i, stiEnabled);
			}
		}
	}
	if (cpu) {
		cpu->NestInterrupt(0);
	}
	AtomicOp::And(activeMask, ~mask);
	return status;
}

/* safe to be called recursively */
int
IM::Poll(int stiEnabled)
{
	IrqClient *ic;

	u32 x = DisableIntr();
	pollLock.Lock();
	if (!AtomicOp::Add(&pollNesting, 1)) {
		roundIdx++;
	}
	pollLock.Unlock();
	while ((ic = SelectClient())) {
		CallClient(ic, stiEnabled);
	}
	AtomicOp::Dec(&pollNesting);
	RestoreIntr(x);
	return 0;
}

int
IM::MaskIrq(IrqType type, u32 idx)
{
	u8 *masked;

	irqmask_t mask = GetMask(idx);
	if (type == IT_HW) {
		assert(mask & hwValid);
		assert(idx <= NUM_HWIRQ);
		masked = &hwMasked[idx];
	} else if (type == IT_SW) {
		assert(mask & swValid);
		assert(idx <= NUM_SWIRQ);
		masked = &swMasked[idx];
	} else {
		panic("Invalid IRQ type %d", type);
	}
	maskLock.Lock();
	/* locked by maskLock so doesn't need to be atomic */
	(*masked)++;
	maskLock.Unlock();
	return 0;
}

int
IM::UnMaskIrq(IrqType type, u32 idx, int stiEnabled)
{
	irqmask_t *pendingMask, *activeMask;
	u8 *masked;

	irqmask_t mask = GetMask(idx);
	/* Process pending interrupt if present */
	if (type == IT_HW) {
		assert(idx <= NUM_HWIRQ);
		assert(mask & hwValid);
		masked = &hwMasked[idx];
		pendingMask = &hwPending;
		activeMask = &hwActive;
	} else if (type == IT_SW) {
		assert(idx <= NUM_SWIRQ);
		assert(mask & swValid);
		masked = &swMasked[idx];
		pendingMask = &swPending;
		activeMask = &swActive;
	} else {
		panic("Invalid IRQ type %d", type);
	}

	maskLock.Lock();
	ensure(*masked);
	u8 isMasked = --(*masked);
	maskLock.Unlock();
	if (!isMasked && (mask & *pendingMask) && !(mask & *activeMask)
		&& pollNesting < MAX_POLL_NESTING) {
		Poll(stiEnabled);
	}
	return 0;
}

Handle
IM::AllocateHwirq(ISR isr, void *arg, u32 idx, u32 flags, int priority)
{
	return (Handle)Allocate(IT_HW, isr, arg, idx, flags, priority);
}

Handle
IM::AllocateSwirq(ISR isr, void *arg, u32 idx, u32 flags, int priority)
{
	return (Handle)Allocate(IT_SW, isr, arg, idx, flags, priority);
}

IM::IrqClient *
IM::Allocate(IrqType type, ISR isr, void *arg, u32 idx, u32 flags, int priority)
{
	IrqSlot *isa;
	u32 numSlots;
	irqmask_t *maskValid;

	if (priority != IP_DEFAULT && priority >= IP_MAX) {
		klog(KLOG_ERROR, "Priority value too big (%d)", priority);
		return 0;
	}

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
	ic->priority = priority;
	LIST_ADD(list, ic, is->clients);
	is->numClients++;
	if (flags & AF_EXCLUSIVE) {
		is->flags |= SF_EXCLUSIVE;
	} else {
		is->flags &= ~SF_EXCLUSIVE;
	}
	*maskValid |= GetMask(idx);
	RecalculatePriorities();
	slotLock.Unlock();
	RestoreIntr(x);
	return ic;
}

u64
IM::SetPL(int priority)
{
	irqmask_t hwMask = GetPLMask(priority, IT_HW);
	irqmask_t swMask = GetPLMask(priority, IT_SW);
	for (u32 idx = 0; idx < NUM_HWIRQ; idx++) {
		irqmask_t mask = GetMask(idx);
		if (hwMask & mask) {
			MaskIrq(IT_HW, idx);
		}
	}
	for (u32 idx = 0; idx < NUM_SWIRQ; idx++) {
		irqmask_t mask = GetMask(idx);
		if (swMask & mask) {
			MaskIrq(IT_SW, idx);
		}
	}
	return ((u64)swMask << 32) | hwMask;
}

int
IM::RestorePL(u64 saved)
{
	irqmask_t hwMask = (irqmask_t)saved;
	irqmask_t swMask = (irqmask_t)(saved >> 32);
	int stiEnabled = GetEflags() & EFLAGS_IF;
	for (u32 idx = 0; idx < NUM_SWIRQ; idx++) {
		irqmask_t mask = GetMask(idx);
		if (swMask & mask) {
			UnMaskIrq(IT_SW, idx, stiEnabled);
		}
	}
	for (u32 idx = 0; idx < NUM_HWIRQ; idx++) {
		irqmask_t mask = GetMask(idx);
		if (hwMask & mask) {
			UnMaskIrq(IT_HW, idx, stiEnabled);
		}
	}
	return 0;
}

/*
 * Recalculates line priorities, masks, sort lists. Client priorities must
 * be set before call. Must be called with slotLock locked.
 */
int
IM::RecalculatePriorities(IrqType type)
{
	u32 numSlots;
	IrqSlot *slots, *is;
	ListHead *slotsHead;
	irqmask_t *plCache;

	if (type == IT_HW) {
		slots = hwIrq;
		numSlots = NUM_HWIRQ;
		slotsHead = &hwSlots;
		plCache = hwPLCache;
	} else if (type == IT_SW) {
		slots = swIrq;
		numSlots = NUM_SWIRQ;
		slotsHead = &swSlots;
		plCache = swPLCache;
	} else {
		panic("Invalid IRQ type: %d", type);
	}

	/* calculate minimal and maximal priority for each line */
	LIST_INIT(*slotsHead);
	for (u32 idx = 0; idx < numSlots; idx++) {
		is = &slots[idx];
		if (!is->numClients) {
			continue;
		}
		LIST_ADD(list, is, *slotsHead);
		int minPri = IP_DEFAULT, maxPri = IP_DEFAULT;
		IrqClient *ic;
		LIST_FOREACH(IrqClient, list, ic, is->clients) {
			if (minPri == IP_DEFAULT || (ic->priority != IP_DEFAULT && ic->priority < minPri)) {
				minPri = ic->priority;
			}
			if (maxPri == IP_DEFAULT || (ic->priority != IP_DEFAULT && ic->priority > maxPri)) {
				maxPri = ic->priority;
			}
		}
		is->minPri = minPri;
		is->maxPri = maxPri;
	}

	/* calculate cached masks values for each priority */
	for (int pl = 0; pl <= IP_MAX; pl++) {
		plCache[pl] = RPGetMask(pl, slotsHead);
	}

	/* sort slots list */
	LIST_SORT(IrqSlot, list, is1, is2, *slotsHead,
		is2->maxPri != IP_DEFAULT && is2->maxPri > is1->maxPri);

	/* sort clients in each slot */
	LIST_FOREACH(IrqSlot, list, is, *slotsHead) {
		LIST_SORT(IrqClient, list, ic1, ic2, is->clients,
			ic2->priority != IP_DEFAULT && ic2->priority > ic1->priority);
		is->first = LIST_FIRST(IrqClient, list, is->clients);
	}
	return 0;
}

IM::irqmask_t
IM::RPGetMask(int priority, ListHead *slots)
{
	irqmask_t mask = 0;

	if (priority == IP_DEFAULT) {
		return 0;
	}
	IrqSlot *is;
	LIST_FOREACH(IrqSlot, list, is, *slots) {
			if (is->minPri == IP_DEFAULT || is->minPri <= priority) {
				mask |= GetMask(is->idx);
			}
	}
	return mask;
}

IM::irqmask_t
IM::GetPLMask(int priority, IrqType type)
{
	if (priority == IP_DEFAULT) {
		return 0;
	}
	irqmask_t *plCache;
	if (type == IT_HW) {
		plCache = hwPLCache;
	} else if (type == IT_SW) {
		plCache = swPLCache;
	} else {
		panic("Invalid IrqClient type (%d)", type);
	}
	return plCache[priority > IP_MAX ? IP_MAX : priority];
}

int
IM::RecalculatePriorities()
{
	int rc = RecalculatePriorities(IT_HW);
	rc |= RecalculatePriorities(IT_SW);
	if (rc) {
		return rc;
	}

	/* set mask for each client */
	IrqSlot *is;
	LIST_FOREACH(IrqSlot, list, is, hwSlots) {
		IrqClient *ic;
		LIST_FOREACH(IrqClient, list, ic, is->clients) {
			irqmask_t mask = GetPLMask(ic->priority, IT_HW);
			mask &= ~GetMask(ic->idx);
			ic->hwMask = mask;
			ic->swMask = GetPLMask(ic->priority, IT_SW);
		}
	}
	LIST_FOREACH(IrqSlot, list, is, swSlots) {
		IrqClient *ic;
		LIST_FOREACH(IrqClient, list, ic, is->clients) {
			irqmask_t mask = GetPLMask(ic->priority, IT_SW);
			mask &= ~GetMask(ic->idx);
			ic->swMask = mask;
			ic->hwMask = GetPLMask(ic->priority, IT_HW);
		}
	}
	return 0;
}

int
IM::ReleaseIrq(Handle h)
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
	RecalculatePriorities();
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
IM::EnableIntr()
{
	u32 rc = GetEflags();
	sti();
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
