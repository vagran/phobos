/*
 * /kernel/make/link.ld
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

ENTRY(start)

SECTIONS {
	. = KERNEL_ADDRESS;
	.boot_trampoline : AT(LOAD_ADDRESS) {
		*/start.o(.text)
	}
	_eboot_tramp = ALIGN(0x1000);
	
	. = LOAD_ADDRESS + _eboot_tramp - KERNEL_ADDRESS;
	.boot : AT (LOAD_ADDRESS + _eboot_tramp - KERNEL_ADDRESS) {
		*/start.o(.data .rodata*)
		*/init.o(.text .data .rodata*)
		_bootbss = ABSOLUTE(.);
		*/start.o(.bss)
		*/init.o(.bss)
	}
	_eboot = ALIGN(0x1000);
	
	. = KERNEL_ADDRESS + _eboot - LOAD_ADDRESS;
	.text : AT(_eboot) {
		_btext = ABSOLUTE(.);
		*(.text)
		*(.gnu.linkonce.t.*)
	}
	
	. = ALIGN(0x1000);
	_etext = ABSOLUTE(.);
	
	/DISCARD/ : {
		*/init.o(.ctors .dtors)
	}
	
	.rodata : AT(_etext - _btext + _eboot) {
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
	
	/* Create RAM-disk from .ramdisk section of input object if exists */
	_ramdisk = _erodata;
	.ramdisk : AT(_erodata - _btext + _eboot) {
		*(.ramdisk)
	}
	
	. = ALIGN(0x1000);
	_eramdisk = ABSOLUTE(.);
	
	.data : AT(_eramdisk - _btext + _eboot) {
		*(.data)
		_ramdiskSize = ABSOLUTE(.);
		LONG(SIZEOF(.ramdisk))
	}
	
	. = ALIGN(0x1000);
	_edata = ABSOLUTE(.);
	
	.bss : AT(_edata - _btext + _eboot) {
		*(.bss)
	}
	
	. = ALIGN(0x1000);
	_end = ABSOLUTE(.);
	
	.stab : AT(_end - _btext + _eboot) {
		*(.stab)
	}
	
	. = ALIGN(0x1000);
	_esym = ABSOLUTE(.);
	
	.stabstr : AT(_esym - _btext + _eboot) {
		*(.stabstr)
	}
	
	. = ALIGN(0x1000);
	_estabstr = ABSOLUTE(.);
	_kernImageEnd = _estabstr;
	
	/DISCARD/ : {
		*(.comment)
	}
}
