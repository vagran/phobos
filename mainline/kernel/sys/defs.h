/*
 * /kernel/sys/defs.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef DEFS_H_
#define DEFS_H_

#define __CONCAT2(x, y)		x##y
#define __CONCAT(x, y)		__CONCAT2(x, y)

#define __STR2(x)			# x
#define __STR(x)			__STR2(x)

#define __UID(str)			__CONCAT(str, __COUNTER__)

#define OFFSETOF(struc_name, field_name) ((unsigned int)&((struc_name *)0)->field_name)

#define BIN(x) ((x & 0x1) | ((x & 0x10) ? 0x2 : 0) | \
	((x & 0x100) ? 0x4 : 0) | ((x & 0x1000) ? 0x8 : 0) | \
	((x & 0x10000) ? 0x10 : 0) | ((x & 0x100000) ? 0x20 : 0) | \
	((x & 0x1000000) ? 0x40 : 0) | ((x & 0x10000000) ? 0x80 : 0))

#define NBBY		8

#define ASMCALL extern "C" __attribute__((regparm(0)))

#define __packed	__attribute__((packed))

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

#define roundup(size, balign)		(((size) + (balign) - 1) / (balign) * (balign))
#define rounddown(size, balign)		((size) / (balign) * (balign))
#define ispowerof2(balign)			((((balign) - 1) & (balign)) == 0)

#define roundup2(size, balign)		(((size) + (balign) - 1) & (~((balign) - 1)))
#define rounddown2(size, balign)	((size) & (~((balign) - 1)))

#if !defined(ASSEMBLER) && defined(KERNEL)
/*
 * Source control system
 */

class phbSourceFile
{
public:
	phbSourceFile(const char *id);
};

#define __phbSourceFileID	__UID(__phbSourceFile)

#define phbSource(id)		volatile static phbSourceFile __phbSourceFileID(id)
#else /* !defined(ASSEMBLER) && defined(KERNEL) */
#define phbSource(id)
#endif /* !defined(ASSEMBLER) && defined(KERNEL) */

phbSource("$Id$");

#endif /* DEFS_H_ */
