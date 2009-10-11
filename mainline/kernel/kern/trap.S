/*
 * /kernel/kern/trap.S
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <frame.h>

#define KCODESEL	(1 << 3)
#define KDATASEL	(2 << 3)

.data
	.globl _trapHandlersTable
_trapHandlersTable:
	.long	0

	.globl _trapHandlersArgs
_trapHandlersArgs:
	.long	0

	.globl _unhandledTrap
_unhandledTrap:
	.long	0

	.globl _unhandledTrapArg
_unhandledTrapArg:
	.long	0

#define TRAP_BODY(idx) \
	PUSHALL; \
	movl	$KDATASEL, %eax; \
	mov		%ax, %ds; \
	mov		%ax, %es; \
	mov		%ax, %fs; \
	mov		%ax, %gs; \
	pushl	%esp; /* arguments for handler */ \
	movl	_trapHandlersArgs, %eax; \
	movl	(idx * 4)(%eax), %eax; \
	pushl	%eax; \
	pushl	$idx; \
	movl	_trapHandlersTable, %eax; \
	movl	(idx * 4)(%eax), %eax; \
	testl	%eax, %eax; \
	jz 		1f; \
	call 	*%eax; \
	jmp		2f; \
1:	movl	_unhandledTrapArg, %eax; \
	movl	%eax, 4(%esp); \
	movl	_unhandledTrap, %eax; \
	call	*%eax; \
2:	addl	$0xc, %esp; \
	cmpl	$KCODESEL, FRAME_CS(%esp); \
	je		1f; \
	call	OnUserRet; \
1:	POPALL; \
	addl	$4,%esp; \
	iret

#define TRAP_CODE(idx) \
	.globl __CONCAT(TrapEntry,idx); \
	__CONCAT(TrapEntry,idx): \
	TRAP_BODY(idx)

#define TRAP_NOCODE(idx) \
	.globl __CONCAT(TrapEntry,idx); \
	__CONCAT(TrapEntry,idx): \
	pushl $0; \
	TRAP_BODY(idx)

.text
TRAP_NOCODE(0x00)
TRAP_NOCODE(0x01)
TRAP_NOCODE(0x02)
TRAP_NOCODE(0x03)
TRAP_NOCODE(0x04)
TRAP_NOCODE(0x05)
TRAP_NOCODE(0x06)
TRAP_NOCODE(0x07)
TRAP_CODE(0x08)
TRAP_NOCODE(0x09)
TRAP_CODE(0x0a)
TRAP_CODE(0x0b)
TRAP_CODE(0x0c)
TRAP_CODE(0x0d)
TRAP_CODE(0x0e)
TRAP_NOCODE(0x0f)
TRAP_NOCODE(0x10)
TRAP_CODE(0x11)
TRAP_NOCODE(0x12)
TRAP_NOCODE(0x13)
