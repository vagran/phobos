/*
 * input.c - low-level input for libelf.
 * Copyright (C) 1995 - 2001, 2005 Michael Riepe
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "private.h"

#ifndef lint
static const char rcsid[] = "@(#) $Id$";
#endif /* lint */

#if HAVE_MMAP
#include <sys/mman.h>
#endif /* HAVE_MMAP */

static int
xread(GFile *fd, char *buffer, size_t len) {
    size_t done = 0;
    size_t n;

    while (done < len) {
		n = fd->Read(buffer + done, len - done);
		if (n == 0) {
			/* premature end of file */
			return -1;
		} else {
			/* some bytes read, continue */
			done += n;
		}
	}
	return 0;
}

void*
_elf_read(Elf *elf, void *buffer, size_t off, size_t len) {
    void *tmp;

	elf_assert(elf);
	elf_assert(elf->e_magic == ELF_MAGIC);
	elf_assert(off >= 0 && off + len <= elf->e_size);
	if (elf->e_disabled) {
		seterr(ERROR_FDDISABLED);
	} else if (len) {
		off += elf->e_base;
		if (elf->e_fd->Seek((off_t)off) != (off_t)off) {
			seterr(ERROR_IO_SEEK);
		} else if (!(tmp = buffer) && !(tmp = malloc(len))) {
			seterr(ERROR_IO_2BIG);
		} else if (xread(elf->e_fd, (char *)tmp, len)) {
			seterr(ERROR_IO_READ);
			if (tmp != buffer) {
				mfree(tmp);
			}
		} else {
			return tmp;
		}
	}
	return NULL;
}

void*
_elf_mmap(Elf *elf) {
#if HAVE_MMAP
    void *tmp;

    elf_assert(elf);
    elf_assert(elf->e_magic == ELF_MAGIC);
    elf_assert(elf->e_base == 0);
    if (elf->e_disabled) {
	seterr(ERROR_FDDISABLED);
    }
    else if (elf->e_size) {
	tmp = (void*)mmap(0, elf->e_size, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE, elf->e_fd, 0);
	if (tmp != (void*)-1) {
	    return tmp;
	}
    }
#endif /* HAVE_MMAP */
    return NULL;
}
