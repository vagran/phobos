/*
 * /phobos/kernel/gate/gate.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#ifndef GATE_H_
#define GATE_H_
#include <sys.h>
phbSource("$Id$");

/* Gate objects description */
#include <gate/App.h>
#include <gate/GateStream.h>
#include <gate/GProcess.h>

#ifdef KERNEL

extern "C" u8 GateEntryStart, GateEntryEnd;
extern "C" u8 GateSysEntry;

#endif /* KERNEL */

#endif /* GATE_H_ */
