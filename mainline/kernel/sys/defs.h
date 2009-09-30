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

#define OFFSETOF(struc_name, field_name) ((unsigned int)&((struc_name *)0)->field_name)

#define ASMCALL extern "C" __attribute__((regparm(0)))

#if defined(__cplusplus) && !defined(ASSEMBLER)
/*
 * Source control system
 */

class phbSourceFile
{
public:
	phbSourceFile(const char *id);
};

#define __phbSourceFileID	__CONCAT(__phbSourceFile, __COUNTER__)

#define phbSource(id)		volatile static phbSourceFile __phbSourceFileID(id)
#else /* defined(__cplusplus) && !defined(ASSEMBLER) */
#define phbSource(id)
#endif /* defined(__cplusplus) && !defined(ASSEMBLER) */

phbSource("$Id$");

#define min(x, y) ((x) <? (y))
#define max(x, y) ((x) >? (y))

#define roundup(size, balign)		(((size) + (balign) - 1) / (balign) * (balign))
#define rounddown(size, balign)		((size) / (balign) * (balign))
#define ispowerof2(balign)			((((balign) - 1) & (balign)) == 0)

#define roundup2(size, balign)		(((size) + (balign) - 1) & (~((balign) - 1)))

#define rounddown2(size, balign)	((size) & (~((balign) - 1)))


#endif /* DEFS_H_ */
