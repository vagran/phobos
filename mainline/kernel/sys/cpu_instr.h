/*
 * /kernel/sys/cpu_instr.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef CPU_INSTR_H_
#define CPU_INSTR_H_
#include <sys.h>
phbSource("$Id$");

static __inline u32
bsf(u32 string)
{
	u32 rc;

	ASM ("bsfl %0, %1" : "=r"(rc) : "r"(string));
	return rc;
}

static __inline u8
inb(u16 port)
{
  u8 _v;

  ASM ("inb %w1,%0" : "=a" (_v):"Nd" (port));
  return _v;
}

static __inline u16
inw(u16 port)
{
  u16 _v;

  ASM ("inw %w1,%0" : "=a" (_v):"Nd" (port));
  return _v;
}

static __inline u32
inl(u16 port)
{
  u32 _v;

  ASM ("inl %w1,%0" : "=a" (_v):"Nd" (port));
  return _v;
}

static __inline void
outb(u16 port, u8 value)
{
  ASM ("outb %b0,%w1" : : "a" (value), "Nd" (port));
}

static __inline void
outw(u16 port, u16 value)
{
  ASM ("outw %w0,%w1": :"a" (value), "Nd" (port));

}

static __inline void
outl(u16 port, u32 value)
{
  ASM ("outl %0,%w1" : : "a" (value), "Nd" (port));
}

static __inline void
invlpg(vaddr_t va)
{
	ASM ("invlpg %0" : : "m"(*(u8 *)va));
}

static __inline u32
rcr0()
{
	register u32 r;

	ASM ("movl %%cr0, %0" : "=r"(r));
	return r;
}

static __inline void
wcr0(u32 x)
{
	ASM ("movl %0, %%cr0" : : "r"(x));
}

static __inline u32
rcr2()
{
	register u32 r;

	ASM ("movl %%cr2, %0" : "=r"(r));
	return r;
}

static __inline void
wcr2(u32 x)
{
	ASM ("movl %0, %%cr2" : : "r"(x));
}

static __inline u32
rcr3()
{
	register u32 r;

	ASM ("movl %%cr3, %0" : "=r"(r));
	return r;
}

static __inline void
wcr3(u32 x)
{
	ASM ("movl %0, %%cr3" : : "r"(x));
}

static __inline u32
rcr4()
{
	register u32 r;

	ASM ("movl %%cr4, %0" : "=r"(r));
	return r;
}

static __inline void
wcr4(u32 x)
{
	ASM ("movl %0, %%cr4" : : "r"(x));
}

static __inline void
cpuid(u32 op, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx)
{
	u32 _eax, _ebx, _ecx, _edx;
	ASM (
		"cpuid"
		: "=a"(_eax), "=b"(_ebx), "=c"(_ecx), "=d"(_edx)
		: "a"(op)
	);
	if (eax) {
		*eax = _eax;
	}
	if (ebx) {
		*ebx = _ebx;
	}
	if (ecx) {
		*ecx = _ecx;
	}
	if (edx) {
		*edx = _edx;
	}
}

static __inline u64
rdtsc()
{
	u64 x;

	ASM ("rdtsc" : "=A"(x));
	return x;
}

#define hlt() ASM ("hlt")

#define cli() ASM ("cli")

#define sti() ASM ("sti")

static __inline void
lgdt(void *p)
{
	ASM (
		"lgdt	%0"
		:
		: "m"(*(u8 *)p)
		);
}

static __inline void
lidt(void *p)
{
	ASM (
		"lidt	%0"
		:
		: "m"(*(u8 *)p)
		);
}

static __inline void
lldt(u16 sel)
{
	ASM (
		"lldt	%0"
		:
		: "r"(sel)
		);
}

static __inline void
sgdt(void *p)
{
	ASM (
		"sgdt	%0"
		: "=m"(*(u8 *)p)
	);
}

static __inline void
sidt(void *p)
{
	ASM (
		"sidt	%0"
		: "=m"(*(u8 *)p)
	);
}

static __inline u16
sldt()
{
	u16 sel;
	ASM (
		"sldt	%0"
		: "=r"(sel)
	);
	return sel;
}

static __inline void
ltr(u16 sel)
{
	ASM (
		"ltr	%0"
		:
		: "r"(sel)
	);
}

static __inline u16
str()
{
	u16 sel;
	ASM (
		"str	%0"
		: "=&r"(sel)
	);
	return sel;
}

static __inline u64
rdmsr(u32 msr)
{
	u64 rc;
	ASM (
		"rdmsr"
		: "=A"(rc)
		: "c"(msr)
	);
	return rc;
}

static __inline void
wrmsr(u32 msr, u64 value)
{
	ASM (
		"wrmsr"
		:
		: "c"(msr), "A"(value)
	);
}

static __inline void
sysenter()
{
	ASM ("sysenter");
}

static __inline void
sysexit(u32 eip, u32 esp)
{
	ASM (
		"sysexit"
		:
		: "d"(eip), "c"(esp)
	);
}

/* pseudo instructions */
static __inline u32
GetEflags()
{
	u32 rc;
	ASM (
		"pushfl\n"
		"popl	%0\n"
		: "=r"(rc)
	);
	return rc;
}

static __inline void
SetEflags(u32 value)
{
	ASM (
		"pushl	%0\n"
		"popfl\n"
		:
		: "r"(value)
	);
}

static __inline void
pause()
{
	ASM ("pause");
}

static __inline void
lfence()
{
	ASM ("lfence");
}

static __inline void
sfence()
{
	ASM ("sfence");
}

static __inline void
mfence()
{
	ASM ("mfence");
}

#endif /* CPU_INSTR_H_ */
