/*
 * /bin/rt_linker/rt_linker.ld
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

SECTIONS {
	. = RT_LINKER_START_RES;
	.rt_linker_res . :{
		. = LOAD_ADDRESS - 1;
	}
}
