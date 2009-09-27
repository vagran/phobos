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


#endif /* DEFS_H_ */
