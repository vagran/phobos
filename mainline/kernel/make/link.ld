/*
 * /kernel/make/link.ld
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

ENTRY(start)

SECTIONS {
	. = LOAD_ADDRESS;
	.lotext : {
		*/start.o(.text)
	}
	_elotext = ALIGN(0x1000);
	
	. = KERNEL_ADDRESS + _elotext - LOAD_ADDRESS;
	.text : AT(_elotext) {
		_btext = ABSOLUTE(.);
		*(.text)
		*(.gnu.linkonce.t.*)
	}
	
	. = ALIGN(0x1000);
	_etext = ABSOLUTE(.);
	
	.rodata : AT(_etext - _btext + _elotext) {
		*(.rodata)
		*(.gnu.linkonce.r.*)
		
		/* place global constructors and destructors list in .rodata section */
		. = ALIGN(4);
		_CTOR_LIST = .;
		LONG((_CTOR_END - _CTOR_LIST) / 4 - 2)
		*(.ctors)
		LONG(0)
		_CTOR_END = .;
		. = ALIGN(4);
		_DTOR_LIST = .;
		LONG((_DTOR_END - _DTOR_LIST) / 4 - 2)
		*(.dtors)
		LONG(0)
		_DTOR_END = .;
	}

	. = ALIGN(0x1000);
	_erodata = ABSOLUTE(.);
	
	.data : AT(_erodata - _btext + _elotext) {
		*(.data)
	}
	
	. = ALIGN(0x1000);
	_edata = ABSOLUTE(.);
	
	.bss : AT(_edata - _btext + _elotext) {
		*(.bss)
	}
	
	. = ALIGN(0x1000);
	_end = ABSOLUTE(.);
	
	.stab : AT(_end - _btext + _elotext) {
		*(.stab)
	}
	
	. = ALIGN(0x1000);
	_esym = ABSOLUTE(.);
	
	.stabstr : AT(_esym - _btext + _elotext) {
		*(.stabstr)
	}
	
	. = ALIGN(0x1000);
	_estabstr = ABSOLUTE(.);
	
	
	/DISCARD/ : {*(.comment)}
}
