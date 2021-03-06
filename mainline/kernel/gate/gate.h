/*
 * /phobos/kernel/gate/gate.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#ifndef GATE_H_
#define GATE_H_
#include <sys.h>
phbSource("$Id$");

/* Gate objects description */
#include <gate/core/App.h>
#include <gate/io/GateStream.h>
#include <gate/io/GConsoleStream.h>
#include <gate/core/GProcess.h>
#include <gate/core/GTime.h>
#include <gate/core/GVFS.h>
#include <gate/core/GEvent.h>

#ifdef KERNEL

extern "C" u8 GateEntryStart, GateEntryEnd;
extern "C" u8 GateSysEntry;

#endif /* KERNEL */

#endif /* GATE_H_ */
