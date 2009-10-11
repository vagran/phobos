/*
 * /kernel/sys/specialreg.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef SPECIALREG_H_
#define SPECIALREG_H_
#include <sys.h>
phbSource("$Id$");

typedef enum {
	EFLAGS_CF =		0x00000001,
	EFLAGS_PF =		0x00000004,
	EFLAGS_AF =		0x00000010,
	EFLAGS_ZF =		0x00000040,
	EFLAGS_SF =		0x00000080,
	EFLAGS_TF =		0x00000100,
	EFLAGS_IF =		0x00000200,
	EFLAGS_DF =		0x00000400,
	EFLAGS_OF =		0x00000800,
	EFLAGS_IOPL =	0x00003000,
	EFLAGS_NT =		0x00004000,
	EFLAGS_RF =		0x00010000,
	EFLAGS_VM =		0x00020000,
	EFLAGS_AC =		0x00040000,
	EFLAGS_VIF =	0x00080000,
	EFLAGS_VIP =	0x00100000,
	EFLAGS_ID =		0x00200000,
}EFLAGSBits;

typedef enum {
/*
 * Bits in 386 special registers:
 */

	CR0_PE =		0x00000001,	/* Protected mode Enable */
	CR0_MP =		0x00000002,	/* "Math" (fpu) Present */
	CR0_EM =		0x00000004,	/* EMulate FPU instructions. (trap ESC only) */
	CR0_TS =		0x00000008,	/* Task Switched (if MP, trap ESC and WAIT) */
	CR0_PG =		0x80000000,	/* PaGing enable */

/*
 * Bits in 486 special registers:
 */
	CR0_NE =		0x00000020,	/* Numeric Error enable (EX16 vs IRQ13) */
	CR0_WP =		0x00010000,	/* Write Protect (honor page protect in
								   all modes) */
	CR0_AM =		0x00040000,	/* Alignment Mask (set to enable AC flag) */
	CR0_NW =		0x20000000,	/* Not Write-through */
	CR0_CD =		0x40000000	/* Cache Disable */
} CR0Bits;

/*
 * Bits in PPro special registers
 */
typedef enum {
	CR4_VME =		0x00000001,	/* Virtual 8086 mode extensions */
	CR4_PVI =		0x00000002,	/* Protected-mode virtual interrupts */
	CR4_TSD =		0x00000004,	/* Time stamp disable */
	CR4_DE =		0x00000008,	/* Debugging extensions */
	CR4_PSE =		0x00000010,	/* Page size extensions */
	CR4_PAE =		0x00000020,	/* Physical address extension */
	CR4_MCE =		0x00000040,	/* Machine check enable */
	CR4_PGE =		0x00000080,	/* Page global enable */
	CR4_PCE =		0x00000100,	/* Performance monitoring counter enable */
	CR4_FXSR =		0x00000200,	/* Fast FPU save/restore used by OS */
	CR4_XMM =		0x00000400,	/* enable SIMD/MMX2 to use except 16 */
} CR4Bits;

typedef enum {
	/*
	 * CPUID instruction features register
	 */
	CPUID_FPU =		0x00000001,
	CPUID_VME =		0x00000002,
	CPUID_DE =		0x00000004,
	CPUID_PSE =		0x00000008,
	CPUID_TSC =		0x00000010,
	CPUID_MSR =		0x00000020,
	CPUID_PAE =		0x00000040,
	CPUID_MCE =		0x00000080,
	CPUID_CX8 =		0x00000100,
	CPUID_APIC =	0x00000200,
	CPUID_B10 =		0x00000400,
	CPUID_SEP =		0x00000800,
	CPUID_MTRR =	0x00001000,
	CPUID_PGE =		0x00002000,
	CPUID_MCA =		0x00004000,
	CPUID_CMOV =	0x00008000,
	CPUID_PAT =		0x00010000,
	CPUID_PSE36 =	0x00020000,
	CPUID_PSN =		0x00040000,
	CPUID_CLFSH =	0x00080000,
	CPUID_B20 =		0x00100000,
	CPUID_DS =		0x00200000,
	CPUID_ACPI =	0x00400000,
	CPUID_MMX =		0x00800000,
	CPUID_FXSR =	0x01000000,
	CPUID_SSE =		0x02000000,
	CPUID_XMM =		0x02000000,
	CPUID_SSE2 =	0x04000000,
	CPUID_SS =		0x08000000,
	CPUID_HTT =		0x10000000,
	CPUID_TM =		0x20000000,
	CPUID_IA64 =	0x40000000,
	CPUID_PBE =		0x80000000,

	CPUID2_SSE3 =		0x00000001,
	CPUID2_DTES64 =		0x00000004,
	CPUID2_MON =		0x00000008,
	CPUID2_DS_CPL =		0x00000010,
	CPUID2_VMX =		0x00000020,
	CPUID2_SMX =		0x00000040,
	CPUID2_EST =		0x00000080,
	CPUID2_TM2 =		0x00000100,
	CPUID2_SSSE3 =		0x00000200,
	CPUID2_CNXTID =		0x00000400,
	CPUID2_CX16 =		0x00002000,
	CPUID2_XTPR =		0x00004000,
	CPUID2_PDCM =		0x00008000,
	CPUID2_DCA =		0x00040000,
	CPUID2_SSE41 =		0x00080000,
	CPUID2_SSE42 =		0x00100000,
	CPUID2_X2APIC =		0x00200000,
	CPUID2_POPCNT =		0x00800000,

	/*
	 * CPUID instruction 1 ebx info
	 */
	CPUID_BRAND_INDEX =		0x000000ff,
	CPUID_CLFUSH_SIZE =		0x0000ff00,
	CPUID_HTT_CORES	=		0x00ff0000,
	CPUID_LOCAL_APIC_ID =	0xff000000,

	/*
	 * CPUID instruction 0xb ebx info.
	 */
	CPUID_TYPE_INVAL =		0,
	CPUID_TYPE_SMT =		1,
	CPUID_TYPE_CORE =		2,
} CPUIDBits;

/*
 * CPUID instruction 1 eax info
 */
#define	CPUID_STEPPING		0x0000000f
#define	CPUID_MODEL			0x000000f0
#define	CPUID_FAMILY		0x00000f00
#define	CPUID_EXT_MODEL		0x000f0000
#define	CPUID_EXT_FAMILY	0x0ff00000,
#define	CPUID_TO_MODEL(id) \
    ((((id) & CPUID_MODEL) >> 4) | \
    ((((id) & CPUID_FAMILY) >= 0x600,) ? \
    (((id) & CPUID_EXT_MODEL) >> 12) : 0))
#define	CPUID_TO_FAMILY(id) \
    ((((id) & CPUID_FAMILY) >> 8) + \
    ((((id) & CPUID_FAMILY) == 0xf00,) ? \
    (((id) & CPUID_EXT_FAMILY) >> 20) : 0))


/*
 * CPUID manufacturers identifiers
 */
#define	AMD_VENDOR_ID		"AuthenticAMD"
#define	CENTAUR_VENDOR_ID	"CentaurHauls"
#define	CYRIX_VENDOR_ID		"CyrixInstead"
#define	INTEL_VENDOR_ID		"GenuineIntel"
#define	NEXGEN_VENDOR_ID	"NexGenDriven"
#define	NSC_VENDOR_ID		"Geode by NSC"
#define	RISE_VENDOR_ID		"RiseRiseRise"
#define	SIS_VENDOR_ID		"SiS SiS SiS "
#define	TRANSMETA_VENDOR_ID	"GenuineTMx86"
#define	UMC_VENDOR_ID		"UMC UMC UMC "

/*
 * Model-specific registers for the i386 family
 */
typedef enum {
	MSR_P5_MC_ADDR =		0x000,
	MSR_P5_MC_TYPE =		0x001,
	MSR_TSC =				0x010,
	MSR_P5_CESR =			0x011,
	MSR_P5_CTR0 =			0x012,
	MSR_P5_CTR1 =			0x013,
	MSR_IA32_PLATFORM_ID =	0x017,
	MSR_APICBASE =			0x01b,
	MSR_EBL_CR_POWERON =	0x02a,
	MSR_TEST_CTL =			0x033,
	MSR_BIOS_UPDT_TRIG =	0x079,
	MSR_BBL_CR_D0 =			0x088,
	MSR_BBL_CR_D1 =			0x089,
	MSR_BBL_CR_D2 =			0x08a,
	MSR_BIOS_SIGN =			0x08b,
	MSR_PERFCTR0 =			0x0c1,
	MSR_PERFCTR1 =			0x0c2,
	MSR_IA32_EXT_CONFIG =	0x0ee,	/* Undocumented. Core Solo/Duo only */
	MSR_MTRRcap =			0x0fe,
	MSR_BBL_CR_ADDR =		0x116,
	MSR_BBL_CR_DECC =		0x118,
	MSR_BBL_CR_CTL =		0x119,
	MSR_BBL_CR_TRIG =		0x11a,
	MSR_BBL_CR_BUSY =		0x11b,
	MSR_BBL_CR_CTL3 =		0x11e,
	MSR_SYSENTER_CS_MSR =	0x174,
	MSR_SYSENTER_ESP_MSR =	0x175,
	MSR_SYSENTER_EIP_MSR =	0x176,
	MSR_MCG_CAP =			0x179,
	MSR_MCG_STATUS =		0x17a,
	MSR_MCG_CTL =			0x17b,
	MSR_EVNTSEL0 =			0x186,
	MSR_EVNTSEL1 =			0x187,
	MSR_THERM_CONTROL =		0x19a,
	MSR_THERM_INTERRUPT =	0x19b,
	MSR_THERM_STATUS =		0x19c,
	MSR_IA32_MISC_ENABLE =	0x1a0,
	MSR_DEBUGCTLMSR =		0x1d9,
	MSR_LASTBRANCHFROMIP =	0x1db,
	MSR_LASTBRANCHTOIP =	0x1dc,
	MSR_LASTINTFROMIP =		0x1dd,
	MSR_LASTINTTOIP =		0x1de,
	MSR_ROB_CR_BKUPTMPDR6 =	0x1e0,
	MSR_MTRRVarBase =		0x200,
	MSR_MTRR64kBase =		0x250,
	MSR_MTRR16kBase =		0x258,
	MSR_MTRR4kBase =		0x268,
	MSR_PAT =				0x277,
	MSR_MTRRdefType =		0x2ff,
	MSR_MC0_CTL =			0x400,
	MSR_MC0_STATUS =		0x401,
	MSR_MC0_ADDR =			0x402,
	MSR_MC0_MISC =			0x403,
	MSR_MC1_CTL =			0x404,
	MSR_MC1_STATUS =		0x405,
	MSR_MC1_ADDR =			0x406,
	MSR_MC1_MISC =			0x407,
	MSR_MC2_CTL =			0x408,
	MSR_MC2_STATUS =		0x409,
	MSR_MC2_ADDR =			0x40a,
	MSR_MC2_MISC =			0x40b,
	MSR_MC3_CTL =			0x40c,
	MSR_MC3_STATUS =		0x40d,
	MSR_MC3_ADDR =			0x40e,
	MSR_MC3_MISC =			0x40f,
	MSR_MC4_CTL =			0x410,
	MSR_MC4_STATUS =		0x411,
	MSR_MC4_ADDR =			0x412,
	MSR_MC4_MISC =			0x413,
} MSRRegs;

typedef enum {
	APICBASE_RESERVED =		0x000006ff,
	APICBASE_BSP =			0x00000100,
	APICBASE_ENABLED =		0x00000800,
	APICBASE_ADDRESS =		0xfffff000,
} APICConsts;

/*
 * PAT modes.
 */
typedef enum {
	PAT_UNCACHEABLE =		0x00,
	PAT_WRITE_COMBINING =	0x01,
	PAT_WRITE_THROUGH =		0x04,
	PAT_WRITE_PROTECTED =	0x05,
	PAT_WRITE_BACK =		0x06,
	PAT_UNCACHED =			0x07
} PATModes;

#define PAT_VALUE(i, m)		((long long)(m) << (8 * (i)))
#define PAT_MASK(i)		PAT_VALUE(i, 0xff)

/*
 * Constants related to MTRRs
 */
typedef enum {
	MTRR_UNCACHEABLE =		0x00,
	MTRR_WRITE_COMBINING =	0x01,
	MTRR_WRITE_THROUGH =	0x04,
	MTRR_WRITE_PROTECTED =	0x05,
	MTRR_WRITE_BACK =		0x06,
	MTRR_N64K =				8,		/* numbers of fixed-size entries */
	MTRR_N16K =				16,
	MTRR_N4K =				64,
	MTRR_CAP_WC =			0x0000000000000400ULL,
	MTRR_CAP_FIXED =		0x0000000000000100ULL,
	MTRR_CAP_VCNT =			0x00000000000000ffULL,
	MTRR_DEF_ENABLE =		0x0000000000000800ULL,
	MTRR_DEF_FIXED_ENABLE =	0x0000000000000400ULL,
	MTRR_DEF_TYPE =			0x00000000000000ffULL,
	MTRR_PHYSBASE_PHYSBASE =	0x000ffffffffff000ULL,
	MTRR_PHYSBASE_TYPE =	0x00000000000000ffULL,
	MTRR_PHYSMASK_PHYSMASK =	0x000ffffffffff000ULL,
	MTRR_PHYSMASK_VALID =	0x0000000000000800ULL,
} MTRRConsts;

/* Performance Control Register (5x86 only). */
typedef enum {
	PCR0 =			0x20,
	PCR0_RSTK =		0x01,	/* Enables return stack */
	PCR0_BTB =		0x02,	/* Enables branch target buffer */
	PCR0_LOOP =		0x04,	/* Enables loop */
	PCR0_AIS =		0x08,	/* Enables all instrcutions stalled to
							   serialize pipe. */
	PCR0_MLR =		0x10,	/* Enables reordering of misaligned loads */
	PCR0_BTBRT =	0x40,	/* Enables BTB test register. */
	PCR0_LSSER =	0x80,	/* Disable reorder */
} PCR0Bits;

/* Device Identification Registers */
typedef enum {
	DIR0 =			0xfe,
	DIR1 =			0xff,
} DIRRegs;

/*
 * Machine Check register constants.
 */
typedef enum {
	MCG_CAP_COUNT =			0x000000ff,
	MCG_CAP_CTL_P =			0x00000100,
	MCG_CAP_EXT_P =			0x00000200,
	MCG_CAP_TES_P =			0x00000800,
	MCG_CAP_EXT_CNT =		0x00ff0000,
	MCG_STATUS_RIPV =		0x00000001,
	MCG_STATUS_EIPV =		0x00000002,
	MCG_STATUS_MCIP =		0x00000004,
	MCG_CTL_ENABLE =		0xffffffffffffffffULL,
	MCG_CTL_DISABLE =		0x0000000000000000ULL,
	MC_STATUS_MCA_ERROR =	0x000000000000ffffULL,
	MC_STATUS_MODEL_ERROR =	0x00000000ffff0000ULL,
	MC_STATUS_OTHER_INFO =	0x01ffffff00000000ULL,
	MC_STATUS_PCC =			0x0200000000000000ULL,
	MC_STATUS_ADDRV =		0x0400000000000000ULL,
	MC_STATUS_MISCV =		0x0800000000000000ULL,
	MC_STATUS_EN =			0x1000000000000000ULL,
	MC_STATUS_UC =			0x2000000000000000ULL,
	MC_STATUS_OVER =		0x4000000000000000ULL,
	MC_STATUS_VAL =			0x8000000000000000ULL,
} MCRegConsts;

#define	MSR_MC_CTL(x)		(MSR_MC0_CTL + (x) * 4)
#define	MSR_MC_STATUS(x)	(MSR_MC0_STATUS + (x) * 4)
#define	MSR_MC_ADDR(x)		(MSR_MC0_ADDR + (x) * 4)
#define	MSR_MC_MISC(x)		(MSR_MC0_MISC + (x) * 4)

/*
 * The following four 3-byte registers control the non-cacheable regions.
 * These registers must be written as three separate bytes.
 *
 * NCRx+0: A31-A24 of starting address
 * NCRx+1: A23-A16 of starting address
 * NCRx+2: A15-A12 of starting address | NCR_SIZE_xx.
 *
 * The non-cacheable region's starting address must be aligned to the
 * size indicated by the NCR_SIZE_xx field.
 */
typedef enum {
	NCR1 =				0xc4,
	NCR2 =				0xc7,
	NCR3 =				0xca,
	NCR4 =				0xcd,

	NCR_SIZE_0K =		0,
	NCR_SIZE_4K =		1,
	NCR_SIZE_8K =		2,
	NCR_SIZE_16K =		3,
	NCR_SIZE_32K =		4,
	NCR_SIZE_64K =		5,
	NCR_SIZE_128K =		6,
	NCR_SIZE_256K =		7,
	NCR_SIZE_512K =		8,
	NCR_SIZE_1M =		9,
	NCR_SIZE_2M =		10,
	NCR_SIZE_4M =		11,
	NCR_SIZE_8M =		12,
	NCR_SIZE_16M =		13,
	NCR_SIZE_32M =		14,
	NCR_SIZE_4G =		15,
} NCRConsts;

/*
 * The address region registers are used to specify the location and
 * size for the eight address regions.
 *
 * ARRx + 0: A31-A24 of start address
 * ARRx + 1: A23-A16 of start address
 * ARRx + 2: A15-A12 of start address | ARR_SIZE_xx
 */
typedef enum {
	ARR0 =				0xc4,
	ARR1 =				0xc7,
	ARR2 =				0xca,
	ARR3 =				0xcd,
	ARR4 =				0xd0,
	ARR5 =				0xd3,
	ARR6 =				0xd6,
	ARR7 =				0xd9,

	ARR_SIZE_0K =		0,
	ARR_SIZE_4K =		1,
	ARR_SIZE_8K =		2,
	ARR_SIZE_16K =		3,
	ARR_SIZE_32K =		4,
	ARR_SIZE_64K =		5,
	ARR_SIZE_128K =		6,
	ARR_SIZE_256K =		7,
	ARR_SIZE_512K =		8,
	ARR_SIZE_1M =		9,
	ARR_SIZE_2M =		10,
	ARR_SIZE_4M =		11,
	ARR_SIZE_8M =		12,
	ARR_SIZE_16M =		13,
	ARR_SIZE_32M =		14,
	ARR_SIZE_4G =		15,
} ARRConsts;
/*
 * The region control registers specify the attributes associated with
 * the ARRx addres regions.
 */
typedef enum {
	RCR0 =			0xdc,
	RCR1 =			0xdd,
	RCR2 =			0xde,
	RCR3 =			0xdf,
	RCR4 =			0xe0,
	RCR5 =			0xe1,
	RCR6 =			0xe2,
	RCR7 =			0xe3,

	RCR_RCD =		0x01,	/* Disables caching for ARRx (x = 0x0,-6). */
	RCR_RCE =		0x01,	/* Enables caching for ARR7. */
	RCR_WWO =		0x02,	/* Weak write ordering. */
	RCR_WL =		0x04,	/* Weak locking. */
	RCR_WG =		0x08,	/* Write gathering. */
	RCR_WT =		10,	/* Write-through. */
	RCR_NLB =		20,	/* LBA# pin is not asserted. */
} RCRConsts;

#endif /* SPECIALREG_H_ */
