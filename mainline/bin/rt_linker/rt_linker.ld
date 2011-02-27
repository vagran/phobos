/*
 * /bin/rt_linker/rt_linker.ld
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

PHDRS
{
	rt_linker_res PT_LOAD FLAGS (0) ;
}

SECTIONS {
	. = RT_LINKER_START_RES;
	.rt_linker_res . :{
		. = LOAD_ADDRESS - 1;
	} :rt_linker_res
}
