/*
 * /make/link.lib.ld
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

PHDRS
{
	headers PT_PHDR FILEHDR PHDRS ;
	text PT_LOAD FLAGS (5) ;
	rodata PT_LOAD FLAGS (4) ;
	data PT_LOAD FLAGS (6) ;
	dynamic PT_DYNAMIC ;
}

SECTIONS {	
	. = LOAD_ADDRESS;
	.text . : {
		*(.text)
		*(.text.*)
		*(.gnu.linkonce.t.*)
	} :text
	
	. = ALIGN(4);
	.init . : {
		*(.init)
		*(.init.*)
	} :text
	
	. = ALIGN(4);
	.fini . : {
		*(.fini)
		*(.fini.*)
	} :text

	. = ALIGN(4);
	.plt . : {
		*(.plt)
	} :text

	. = ALIGN(0x1000);

	.rodata . : {
		*(.rodata*)
		*(.gnu.linkonce.r.*)
	} :rodata

	. = ALIGN(4);
	.dynamic . : { *(.dynamic) } :rodata :dynamic

	. = ALIGN(4);
	.hash . : { *(.hash) } :rodata

	. = ALIGN(4);
	.dynsym . : { *(.dynsym) } :rodata

	. = ALIGN(4);
	.dynstr . : { *(.dynstr) } :rodata

	. = ALIGN(4);
	.rel.dyn . : { *(.rel.dyn) } :rodata

	. = ALIGN(4);
	.rel.plt . : { *(.rel.plt) } :rodata

	. = ALIGN(0x1000);
	
	.data . : {
		*(.data)
	} :data
	
	. = ALIGN(4);
	.data.rel.ro . : { *(.data.rel.ro.*) } :data

	. = ALIGN(4);
	.got . : {
		*(.got)
	} :data

	. = ALIGN(4);
	.got.plt . : {
		*(.got.plt)
	} :data

	/* place global constructors and destructors list in writable section */
	. = ALIGN(4);
	.ctors . : {
		_SL_CTOR_LIST = .;
		LONG((_SL_CTOR_END - _SL_CTOR_LIST) / 4 - 2)
		*(SORT(.ctors))
		LONG(0)
		_SL_CTOR_END = .;
	} :data
	. = ALIGN(4);
	.dtors . : {
		_SL_DTOR_LIST = .;
		LONG((_SL_DTOR_END - _SL_DTOR_LIST) / 4 - 2)
		*(SORT(.dtors))
		LONG(0)
		_SL_DTOR_END = .;
	} :data
	
	. = ALIGN(0x1000);

	.bss . : {
		*(.bss)
	} :data

	. = ALIGN(0x1000);

	/DISCARD/ : {
		*(.eh_frame)
		*(.comment)
	}
}
