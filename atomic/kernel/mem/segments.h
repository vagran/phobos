/*
 * /kernel/mem/segments.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#ifndef SEGMENTS_H_
#define SEGMENTS_H_
#include <sys.h>
phbSource("$Id$");

class SDT : public Object {
public:
	struct SegSelector_s {
		u32	rpl:2,		/* request privilege level */
			ti:1,		/* table index : 0 select gdt, 1 select ldt */
			idx:13;		/* index */
	} __packed;
	typedef struct SegSelector_s SegSelector;

	struct PseudoDescriptor_s {
		u16	limit;
		u32	base;
	} __packed;
	typedef struct PseudoDescriptor_s PseudoDescriptor;

	struct Descriptor_s {
		u32 lolimit:16;		/* segment extent (lsb) */
		u32 lobase:24;		/* segment base address (lsb) */
		u32 type:4;			/* segment type */
		u32 data:1;			/* data/code(1) or system(0) */
		u32 dpl:2;			/* segment descriptor priority level */
		u32 p:1;			/* segment descriptor present */
		u32 hilimit:4;		/* segment extent (msb) */
		u32 unused:2;		/* unused */
		u32 def32:1;		/* default 32 vs 16 bit size */
		u32 gran:1;			/* limit granularity (byte/page units)*/
		u32 hibase:8 ;		/* segment base address (msb) */
	} __packed;
	typedef struct Descriptor_s Descriptor;

	struct Gate_s {
		u32 looffset:16;
		u32	selector:16;
		u32	paramCount:5;
		u32 unused:3;
		u32	type:4;
		u32	data:1;			/* must be 0 */
		u32	dpl:2;
		u32 p:1;
		u32 hioffset:16;
	} __packed;
	typedef struct Gate_s Gate;

	/* system segments and gate types */
	typedef enum {
		SST_RESERVED1 =		0,	/* reserved */
		SST_TSS16 =			1,	/* 16-bit TSS available */
		SST_LDT =			2,	/* local descriptor table */
		SST_TSS16BSY =		3,	/* 16-bit TSS busy */
		SST_CGT16 =			4,	/* 16-bit call gate */
		SST_TASKGT =		5,	/* task gate */
		SST_IGT16 =			6,	/* 16-bit interrupt gate */
		SST_TGT16 =			7,	/* 16-bit trap gate */
		SST_RESERVED2 =		8,	/* reserved */
		SST_TSS32 =			9,	/* 32-bit TSS available */
		SST_RESERVED3 =		10,	/* reserved */
		SST_TSS32BSY =		11,	/* 32-bit TSS busy */
		SST_CGT32 =			12,	/* 32-bit call gate */
		SST_RESERVED4 =		13,	/* reserved */
		SST_IGT32 =			14,	/* 32-bit interrupt gate */
		SST_TGT32 =			15,	/* 32-bit trap gate */
	} SysSegType;

	/* code- and data-segment descriptor types */
	typedef enum {
		DST_DATA =			0x0,
		DST_CODE =			0x8,
		DST_ACCESSED =		0x1,
		DST_D_WRITE =		0x2,
		DST_D_EXPANDDOWN =	0x4,
		DST_C_READ =		0x2,
		DST_C_CONFORMING =	0x4,
	} DataSegType;

public:
	static void SetDescriptor(Descriptor *d, u32 base, u32 limit, u32 ring, u32 system,
		u32 type, int is16bit = 0);
	static void SetGate(Gate *g, u32 offset, u16 selector, u32 type);
	static u32 GetBase(Descriptor *d);
	static u32 GetLimit(Descriptor *d);
};

class GDT : public Object {
public:
	/*
	 * Kernel code and data among with user code and data segments should have
	 * fixed selector positions relatively to each other. This is required for
	 * SYSENTER/SYSEXIT instructions support.
	 */
	typedef enum {
		SI_NULL,
		SI_KCODE,
		SI_KDATA,
		SI_UCODE,
		SI_UDATA,
		SI_LDT,
		SI_TSS,
		SI_CUSTOM,
	} SelIdx;

	typedef enum {
		PL_KERNEL =		0,
		PL_USER =		3,
	} PriorityLevel;
private:
	enum {
		GDT_SIZE =		512,
	};

	typedef struct {
		SDT::Descriptor
			null,
			kcode,
			kdata,
			ucode,
			udata,
			ldt,
			tss;
	} Table;
	Table *table;

	typedef struct {
		SDT::Descriptor
			null;
	} ldtTable;
	ldtTable *ldt;

	SDT::PseudoDescriptor pd;
	u32 GetIndex(SDT::Descriptor *d);
public:
	GDT();

	static inline u16 GetSelector(u32 idx, u16 rpl = PL_KERNEL) { return (idx << 3) | rpl; }
	static inline PriorityLevel GetRPL(u16 selector) { return (PriorityLevel)(selector & 3); }
	static inline u32 GetIndex(u16 selector) { return selector >> 3; }
	u16 GetSelector(SDT::Descriptor *d, u16 rpl = PL_KERNEL);
	SDT::Descriptor *AllocateSegment();
	int ReleaseSegment(SDT::Descriptor *d);
	SDT::PseudoDescriptor *GetPseudoDescriptor() { return &pd; }
	inline SDT::Descriptor *GetDescriptor(u32 index) { return &table->null + index; }
	inline SDT::Descriptor *GetDescriptor(u16 selector) { return GetDescriptor(GetIndex(selector)); }
};

extern GDT *gdt;

class IDT : public Object {
public:
	typedef enum {
		ST_DIVIDE,
		ST_DEBUG,
		ST_NMI,
		ST_BREAKPOINT,
		ST_OVERFLOW,
		ST_BOUNDRANGE,
		ST_INVOPCODE,
		ST_DEVNOTAVAIL,
		ST_DOUBLEFAULT,
		ST_CPSEGOVERRUN,
		ST_INVTSS,
		ST_SEGNOTPRESENT,
		ST_STACKSEGFAULT,
		ST_GENPROT,
		ST_PAGEFAULT,
		ST_RESERVED2,
		ST_FPU,
		ST_ALIGNMENT,
		ST_MACHINECHECK,
		ST_SIMD,
		ST_NUMRESERVED = 32, /* reserved by Intel */
	} SysTraps;

	typedef int (Object::*TrapHandler)(Frame *frame);

private:
	enum {
		NUM_VECTORS =		0x100,
	};

	SDT::Gate *table;
	SDT::PseudoDescriptor pd;
	typedef struct {
		TrapHandler handler;
		Object *obj;
	} TableEntry;
	TableEntry handlers[NUM_VECTORS];
	TableEntry utHandler;
	u32 trapEntrySize;

public:
	static const char *StrTrap(SysTraps trap);

	IDT();
	int HandleTrap(Frame *frame);

	TrapHandler RegisterHandler(u32 idx, Object *obj, TrapHandler h);
	TrapHandler RegisterUTHandler(Object *obj, TrapHandler h);
	SDT::PseudoDescriptor *GetPseudoDescriptor() { return &pd; }
};

class TSS : public Object {
private:
	typedef struct {
		u32 tss_link; /* actually 16 bits: top 16 bits must be zero */
		u32 tss_esp0; /* kernel stack pointer privilege level 0 */
		u32 tss_ss0; /* actually 16 bits: top 16 bits must be zero */
		u32 tss_esp1; /* kernel stack pointer privilege level 1 */
		u32 tss_ss1; /* actually 16 bits: top 16 bits must be zero */
		u32 tss_esp2; /* kernel stack pointer privilege level 2 */
		u32 tss_ss2; /* actually 16 bits: top 16 bits must be zero */
		u32 tss_cr3; /* page table directory */
		u32 tss_eip; /* program counter */
		u32 tss_eflags; /* program status longword */
		u32 tss_eax;
		u32 tss_ecx;
		u32 tss_edx;
		u32 tss_ebx;
		u32 tss_esp; /* user stack pointer */
		u32 tss_ebp; /* user frame pointer */
		u32 tss_esi;
		u32 tss_edi;
		u32 tss_es; /* actually 16 bits: top 16 bits must be zero */
		u32 tss_cs; /* actually 16 bits: top 16 bits must be zero */
		u32 tss_ss; /* actually 16 bits: top 16 bits must be zero */
		u32 tss_ds; /* actually 16 bits: top 16 bits must be zero */
		u32 tss_fs; /* actually 16 bits: top 16 bits must be zero */
		u32 tss_gs; /* actually 16 bits: top 16 bits must be zero */
		u32 tss_ldt; /* actually 16 bits: top 16 bits must be zero */
		u32 tss_ioopt; /* options and I/O offset bitmap: currently zero */
	} TssData;

	typedef struct {
		TssData data;
		TSS *tss;
		u8 privateData[0];
	} TssSegment;

	SDT::Descriptor *desc;
	TssSegment *data;
	u32 dataSize;

public:
	TSS(void *kernelStack, u32 privateDataSize = 0);

	int SetActive();
	void *GetPrivateData(u32 *size = 0);
	static TSS *GetCurrent();
};

extern IDT *idt;
extern TSS *defTss;
extern "C" u8 TrapEntry, TrapEntryEnd;
extern "C" void *TrapTable;

#endif /* SEGMENTS_H_ */
