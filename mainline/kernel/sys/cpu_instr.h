/*
 * /kernel/sys/cpu_instr.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#ifndef CPU_INSTR_H_
#define CPU_INSTR_H_
#include <sys.h>
phbSource("$Id$");

static __inline u8
inb(u16 port)
{
  u8 _v;

  __asm__ __volatile__ ("inb %w1,%0":"=a" (_v):"Nd" (port));
  return _v;
}

static __inline u16
inw(u16 port)
{
  u16 _v;

  __asm__ __volatile__ ("inw %w1,%0":"=a" (_v):"Nd" (port));
  return _v;
}

static __inline u32
inl(u16 port)
{
  u32 _v;

  __asm__ __volatile__ ("inl %w1,%0":"=a" (_v):"Nd" (port));
  return _v;
}

static __inline void
outb(u8 value, u16 port)
{
  __asm__ __volatile__ ("outb %b0,%w1": :"a" (value), "Nd" (port));
}

static __inline void
outw(u16 value, u16 port)
{
  __asm__ __volatile__ ("outw %w0,%w1": :"a" (value), "Nd" (port));

}

static __inline void
outl(u32 value, u16 port)
{
  __asm__ __volatile__ ("outl %0,%w1": :"a" (value), "Nd" (port));
}

static __inline void
invlpg(vaddr_t va)
{
	__asm__ __volatile__ ("invlpg %0"::"m"(*(u8 *)va));
}

static __inline u32
rcr0()
{
	register u32 r;

	__asm__ __volatile__ ("movl %%cr0, %0" : "=r"(r));
	return r;
}

static __inline void
wcr0(u32 x)
{
	__asm__ __volatile__ ("movl %0, %%cr0" : : "r"(x));
}

static __inline u32
rcr2()
{
	register u32 r;

	__asm__ __volatile__ ("movl %%cr2, %0" : "=r"(r));
	return r;
}

static __inline void
wcr2(u32 x)
{
	__asm__ __volatile__ ("movl %0, %%cr2" : : "r"(x));
}

static __inline u32
rcr3()
{
	register u32 r;

	__asm__ __volatile__ ("movl %%cr3, %0" : "=r"(r));
	return r;
}

static __inline void
wcr3(u32 x)
{
	__asm__ __volatile__ ("movl %0, %%cr3" : : "r"(x));
}

static __inline u32
rcr4()
{
	register u32 r;

	__asm__ __volatile__ ("movl %%cr4, %0" : "=r"(r));
	return r;
}

static __inline void
wcr4(u32 x)
{
	__asm__ __volatile__ ("movl %0, %%cr4" : : "r"(x));
}

static __inline void
cpuid(u32 op, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx)
{
	__asm__ __volatile__ (
		"cpuid" :
		"=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) :
		"a"(op));
}

static __inline u64
rdtsc()
{
	u64 x;

	__asm__ __volatile__ ("rdtsc" : "=A"(x));
	return x;
}

#define hlt() __asm__ __volatile__ ("hlt")

#define cli() __asm__ __volatile__ ("cli")

#define sti() __asm__ __volatile__ ("sti")

static __inline void
lgdt(void *p)
{
	__asm__ __volatile__ (
		"lgdt	%0"
		:
		: "m"(*(u8 *)p)
		);
}

static __inline void
lidt(void *p)
{
	__asm__ __volatile__ (
		"lidt	%0"
		:
		: "m"(*(u8 *)p)
		);
}

static __inline void
lldt(u16 sel)
{
	__asm__ __volatile__ (
		"lldt	%0"
		:
		: "r"(sel)
		);
}

static __inline void
sgdt(void *p)
{
	__asm__ __volatile__ (
		"sgdt	%0"
		: "=m"(*(u8 *)p)
	);
}

static __inline void
sidt(void *p)
{
	__asm__ __volatile__ (
		"sidt	%0"
		: "=m"(*(u8 *)p)
	);
}

static __inline u16
sldt()
{
	u16 sel;
	__asm__ __volatile__ (
		"sldt	%0"
		: "=r"(sel)
	);
	return sel;
}

#endif /* CPU_INSTR_H_ */