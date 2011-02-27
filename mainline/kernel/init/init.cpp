/*
 * /kernel/init/init.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#include <sys.h>
phbSource("$Id$");

/*
 * This file is only file which cannot use statically allocated objects,
 * since its construction/destruction code is discarded by the linker.
 * Also it cannot use any code from any of the rest files, because
 * it is relocated at low bootstrapping memory but all the rest system
 * is designed to be run in higher virtual addresses. Prior to pass execution
 * to the rest code, initial memory mapping must be done and paging enabled.
 */

#include <boot.h>
#include <mem.h>

#define INITIAL_STACK_SIZE		16384
static PTE::PDEntry *bsIdlePTD;
static PTE::PDPTEntry *bsIdlePDPT;
static paddr_t bsQuickMap;
static PTE::PTEntry *bsQuickMapPTE;

#ifdef SERIAL_DEBUG
#include <dev/chr/ser.h>

#define SERIAL_PORT		0x3f8

static void
dbg_putc(u32 c)
{
	u32 timeout = 100000;

	if (c=='\n') {
		dbg_putc('\r');
	}
	/* Wait until the transmitter holding register is empty.  */
	while (!(inb(SERIAL_PORT + SerialPort::UART_LSR) & SerialPort::UART_EMPTY_TRANSMITTER)) {
		if (!--timeout) {
			return;
		}
		pause();
	}
	outb(SERIAL_PORT + SerialPort::UART_TX, c);
}

static void
dbg_puts(char *s)
{
	while (*s) {
		dbg_putc(*s++);
	}
}

static void
dbg_print_num(u32 num, u32 base)
{
	if (num >= base) {
		dbg_print_num(num / base, base);
		num = num % base;
	}
	if (num < 10) {
		dbg_putc('0' + num);
	} else {
		dbg_putc('a' + num - 10);
	}
}

static void
dbg_vprintf(const char *fmt,va_list va)
{
	while (*fmt) {
		if (*fmt != '%') {
			dbg_putc(*fmt++);
		} else {
			fmt++;
			if (!*fmt) {
				dbg_putc('%');
				break;
			}
			switch (*fmt) {
			case 'd': {
				u32 x = va_arg(va, u32);
				dbg_print_num(x, 10);
				break;
			}
			case 'x': {
				u32 x = va_arg(va, u32);
				dbg_print_num(x, 16);
				break;
			}
			case 's': {
				char *s = va_arg(va, char *);
				dbg_puts(s);
				break;
			}
			default:
				dbg_putc(*fmt);
			}
			fmt++;
		}
	}
}

static void
dbg_printf(const char *fmt,...)
{
	va_list va;
	va_start(va, fmt);
	dbg_vprintf(fmt, va);
}

static void
dbgTraceTranslation(vaddr_t va)
{
	dbg_printf("tracing VA 0x%x\n", va);
	PTE::PDPTEntry *pdpte = &bsIdlePDPT[va >> PDPT_SHIFT];
	dbg_printf("PDPTE [0x%x] = 0x%x\n", va >> PDPT_SHIFT, (u32)pdpte->raw);
	PTE::PDEntry *pde = (PTE::PDEntry *)(pdpte->raw & PG_FRAME) +
		((va & PD_MASK) >> PD_SHIFT);
	dbg_printf("PDE [0x%x] = 0x%x\n", (va & PD_MASK) >> PD_SHIFT, (u32)pde->raw);
	PTE::PTEntry *pte = (PTE::PTEntry *)(pde->raw & PG_FRAME) +
		((va & PT_MASK) >> PT_SHIFT);
	dbg_printf("PTE [0x%x] = 0x%x\n", (va & PT_MASK) >> PT_SHIFT, (u32)pte->raw);
	paddr_t pa = (pte->raw & PG_FRAME) | (va & ~PG_FRAME);
	dbg_printf("PA = 0x%x\n", (u32)pa);
}

#define TRACE(s,...)	dbg_printf(s, ## __VA_ARGS__)
#define TRACELAT(name, va) { \
	dbg_printf("%s:\n", name); \
	dbgTraceTranslation(va); \
}
#else /* SERIAL_DEBUG */

#define TRACE(s,...)
#define TRACELAT(name, va)
#endif /* SERIAL_DEBUG */

static void
bs_panic(const char *msg,...)
{
#ifdef SERIAL_DEBUG
	dbg_printf("panic: ");
	va_list va;
	va_start(va, msg);
	dbg_vprintf(msg, va);
#endif
	hlt();
}

/* make all dynamic allocations right after kernel image end */
static u8 *bsMemory = &_kernImageEnd - KERNEL_ADDRESS + LOAD_ADDRESS;

#define BSMAPMEM()	{while (AddPT((paddr_t)bsMemory));}

static int bsPagingEnabled = 0;

static void *
bs_malloc(u32 size, u32 align = 4)
{
	bsMemory = (u8 *)roundup((u32)bsMemory, align);
	void *p = bsMemory;
	bsMemory += size;
	return p;
}

static u32
bs_strlen(char *s)
{
	u32 len = 0;
	while (*s) {
		len++;
		s++;
	}
	return len;
}

static void
bs_memcpy(void *dst, void *src, u32 size)
{
	for (u32 i = 0; i < size; i++) {
		((u8 *)dst)[i] = ((u8 *)src)[i];
	}
}

static void
bs_memset(void *dst, u8 fill, u32 size)
{
	for (u32 i = 0; i < size; i++) {
		((u8 *)dst)[i] = fill;
	}
}

static paddr_t bsMapped = LOAD_ADDRESS;

static void *
bsQuickMapEnter(paddr_t pa)
{
	pa = rounddown2(pa, PAGE_SIZE);
	bsQuickMapPTE->raw = pa | PTE::F_W | PTE::F_P;
	invlpg(pa);
	return (void *)bsQuickMap;
}

/* ret non-zero if page table(s) added, zero otherwise */
static int
AddPT(paddr_t pa)
{
	int wasAdded = 0;

	pa = roundup2(pa, PAGE_SIZE);

	while (bsMapped < pa) {
		PTE::PTEntry *pte;
		PTE::PDEntry *pde = &bsIdlePTD[bsMapped >> PD_SHIFT];
		if (!pde->fields.present) {
			wasAdded = 1;
			pte = (PTE::PTEntry *)bs_malloc(PAGE_SIZE, PAGE_SIZE);
			pde->raw = (paddr_t)pte | PTE::F_U | PTE::F_W | PTE::F_P;
			/* map to both low and high memory */
			pde = &bsIdlePTD[(bsMapped - LOAD_ADDRESS + KERNEL_ADDRESS) >> PD_SHIFT];
			pde->raw = (paddr_t)pte | PTE::F_W | PTE::F_P;
			if (bsPagingEnabled) {
				pte = (PTE::PTEntry *)bsQuickMapEnter((paddr_t)pte);
			}
			bs_memset(pte, 0, PAGE_SIZE);
		} else {
			pte = (PTE::PTEntry *)(pde->raw & PG_FRAME);
		}
		pte[(bsMapped & PT_MASK) >> PT_SHIFT].raw = bsMapped | PTE::F_W | PTE::F_P;
		bsMapped += PAGE_SIZE;
	}
	return wasAdded;
}

static void
CreateInitialMapping()
{
	if (LOAD_ADDRESS & ((1 << PD_SHIFT) - 1)) {
		bs_panic("LOAD_ADDRESS not PTD aligned: 0x%x", LOAD_ADDRESS);
	}

	bsIdlePTD = (PTE::PDEntry *)bs_malloc(PD_PAGES * PAGE_SIZE, PAGE_SIZE);
	bs_memset(bsIdlePTD, 0, PD_PAGES * PAGE_SIZE);
	bsIdlePDPT = (PTE::PDPTEntry *)bs_malloc(PD_PAGES * sizeof(PTE::PDPTEntry),
		PD_PAGES * sizeof(PTE::PDPTEntry));
	bs_memset(bsIdlePDPT, 0, PD_PAGES * sizeof(PTE::PDPTEntry));
	for (int i = 0; i < PD_PAGES; i++) {
		bsIdlePDPT[i].fields.present = 1;
		bsIdlePDPT[i].fields.physAddr = atop((u64)bsIdlePTD) + i;
	}

	/* create space for quick maps */
	bsQuickMap = (paddr_t)bs_malloc(MM::QUICKMAP_SIZE * PAGE_SIZE, PAGE_SIZE);
	BSMAPMEM(); /* map all used memory */
	PTE::PDEntry *pde = &bsIdlePTD[bsQuickMap >> PD_SHIFT];
	bsQuickMapPTE = (PTE::PTEntry *)(pde->raw & PG_FRAME) +
		((bsQuickMap & PT_MASK) >> PT_SHIFT);
}

static void
CreateInitialStack()
{
	if (INITIAL_STACK_SIZE > PT_ENTRIES * PAGE_SIZE) {
		bs_panic("Initial stack size limit exceeded: %d of %d max",
			INITIAL_STACK_SIZE, PT_ENTRIES * PAGE_SIZE);
	}
	PTE::PTEntry *pte = (PTE::PTEntry *)bs_malloc(PAGE_SIZE, PAGE_SIZE);
	paddr_t stackPBase = (paddr_t)bs_malloc(INITIAL_STACK_SIZE, PAGE_SIZE);
	BSMAPMEM();
	u32 pteIdx = PT_ENTRIES - INITIAL_STACK_SIZE / PAGE_SIZE;
	for (u32 i = pteIdx; i < PT_ENTRIES; i++) {
		pte[i].raw = (stackPBase + (i - pteIdx) * PAGE_SIZE) | PTE::F_W | PTE::F_P;
	}
	/* stack is located below KERNEL_ADDRESS */
	PTE::PDEntry *pde = &bsIdlePTD[(KERNEL_ADDRESS - PAGE_SIZE) >> PD_SHIFT];
	pde->raw = (paddr_t)pte | PTE::F_U | PTE::F_W | PTE::F_P;

	/* copy old stack content to new stack and switch stack pointer */
	__asm __volatile (
		"movl	%%esp, %%esi\n"
		"movl	$boot_stack, %%ecx\n"
		"subl	%%esi, %%ecx\n"
		"movl	$KERNEL_ADDRESS, %%edi\n"
		"subl	%%ecx, %%edi\n"
		"movl	%%edi, %%esp\n"
		"cld\n"
		"rep\n"
		"movsb\n"
		:
		:
		: "ecx", "esi", "edi", "cc"
	);
}

static MBInfo *
LoadMBInfo(MBInfo *p)
{
	MBInfo *out = (MBInfo *)bs_malloc(sizeof(MBInfo));
	bs_memcpy(out, p, sizeof(MBInfo));
	if (out->flags & MBIF_CMDLINE) {
		u32 len = bs_strlen(p->cmdLine) + 1;
		out->cmdLine = (char *)bs_malloc(len);
		bs_memcpy(out->cmdLine, p->cmdLine, len);
		TRACE("Command line: '%s'\n", out->cmdLine);
		out->cmdLine += KERNEL_ADDRESS - LOAD_ADDRESS;
	}
	if (out->flags & MBIF_MEMMAP) {
		void *buf = bs_malloc(p->mmapLength);
		bs_memcpy(buf, p->mmapAddr, p->mmapLength);
		out->mmapAddr = (MBIMmapEntry *)((vaddr_t)buf - LOAD_ADDRESS + KERNEL_ADDRESS);
	}
	return out;
}

ASMCALL void
Bootstrap(u32 mbSignature, MBInfo *mbi)
{
	u8 *p;
	MBInfo *pmbi = 0;

	/* zero bootstrap bss */
	for (p = &_bootbss; p < &_eboot; p++) {
		*p = 0;
	}
	TRACE("Bootstrapping started\n");

	if (mbSignature == MULTIBOOT_BOOTLOADER_MAGIC) {
		pmbi = LoadMBInfo(mbi);
	}

	CreateInitialMapping();
#ifdef DEBUG
	TRACELAT("LOAD_ADDRESS", LOAD_ADDRESS);
	TRACELAT("KERNEL_ADDRESS", KERNEL_ADDRESS);
#endif /* DEBUG */

	TRACE("Enabling paging...");
	wcr3((u32)bsIdlePDPT);
	wcr4(rcr4() | CR4_PAE);
	wcr0(rcr0()	| CR0_PG);
	TRACE("ok\n");
	bsPagingEnabled = 1;

	/* zero bss */
	for (p = &_edata; p < &_end; p++) {
		*p = 0;
	}

	IdlePDPT = (paddr_t)bsIdlePDPT;
	vIdlePDPT = (vaddr_t)bsIdlePDPT - LOAD_ADDRESS + KERNEL_ADDRESS;
	IdlePTD = (paddr_t)bsIdlePTD;
	vIdlePTD = (vaddr_t)bsIdlePTD - LOAD_ADDRESS + KERNEL_ADDRESS;
	pMBInfo = pmbi ? (MBInfo *)((u8 *)pmbi - LOAD_ADDRESS + KERNEL_ADDRESS) : 0;
	quickMap = (vaddr_t)(bsQuickMap - LOAD_ADDRESS + KERNEL_ADDRESS);

	CreateInitialStack();
#ifdef DEBUG
	TRACELAT("Initial stack", KERNEL_ADDRESS - PAGE_SIZE);
	TRACE("IdlePDPT = 0x%x, IdlePTD = 0x%x\n", (u32)IdlePDPT, (u32)IdlePTD);
	TRACELAT("IdlePTD recursive",
		(vaddr_t)((PTE::PDEntry *)IdlePTD + PD_PAGES * PT_ENTRIES - PD_PAGES));
#endif /* DEBUG */

	Main((paddr_t)bsMemory);
}
