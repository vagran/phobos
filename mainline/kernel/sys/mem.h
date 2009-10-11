/*
 * /kernel/sys/mem.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef MEM_H_
#define MEM_H_
#include <sys.h>
phbSource("$Id$");

#include <mem/pte.h>
#include <mem/mm.h>
#include <mem/segments.h>

extern paddr_t IdlePDPT, IdlePTD;
extern vaddr_t quickMap;

#endif /* MEM_H_ */
