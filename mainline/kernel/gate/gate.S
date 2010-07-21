/*
 * /phobos/kernel/gate/gate.S
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

/* unified gate entry, gate objects' dispatch tables consist of these entries */

.globl GateEntryStart, GateEntryEnd
GateEntryStart:
	call	1f
1:	popl	%edx		/* pass %eip in %edx */
	movl	%esp, %eax	/* and %esp in %eax */
	sysenter
GateEntryEnd:

/* Kernel system calls entry point */

.globl GateSysEntry
GateSysEntry:
