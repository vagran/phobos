/*
 * /make/link.app.ld
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */
 
 ENTRY(AppStart)
 
 SECTIONS {
	. = LOAD_ADDRESS;
	.text . :{
		*(.text)
		*(.text.*)
		*(.gnu.linkonce.t.*)
	}
	
	. = ALIGN(0x1000);
	_etext = ABSOLUTE(.);
	
	.rodata . : {
		*(.rodata*)
		*(.gnu.linkonce.r.*)
		
		/* place global constructors and destructors list in .rodata section */
		. = ALIGN(4);
		_CTOR_LIST = .;
		LONG((_CTOR_END - _CTOR_LIST) / 4 - 2)
		*(SORT(.ctors))
		LONG(0)
		_CTOR_END = .;
		. = ALIGN(4);
		_DTOR_LIST = .;
		LONG((_DTOR_END - _DTOR_LIST) / 4 - 2)
		*(SORT(.dtors))
		LONG(0)
		_DTOR_END = .;
	}

	. = ALIGN(0x1000);
	_erodata = ABSOLUTE(.);
	
	.data . : {
		*(.data)
	}
	
	. = ALIGN(0x1000);
	_edata = ABSOLUTE(.);
	
	.bss . : {
		*(.bss)
	}
	
	. = ALIGN(0x1000);
	_end = ABSOLUTE(.);
	
	.stab . : {
		*(.stab)
	}
	
	. = ALIGN(0x1000);
	_esym = ABSOLUTE(.);
	
	.stabstr . : {
		*(.stabstr)
	}
	
	. = ALIGN(0x1000);
	_estabstr = ABSOLUTE(.);
	
	/DISCARD/ : {
		*(.comment)
	}
}
