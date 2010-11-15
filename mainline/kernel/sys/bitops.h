/*
 * /kernel/sys/bitops.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#ifndef BITOPS_H_
#define BITOPS_H_
#include <sys.h>
phbSource("$Id$");

#define	BitSet(a, i)		(((u8 *)(a))[(i) / NBBY] |= 1 << ((i) % NBBY))
#define	BitClear(a, i)		(((u8 *)(a))[(i) / NBBY] &= ~(1 << ((i) % NBBY)))
#define	BitIsSet(a, i)		(((const u8 *)(a))[(i) / NBBY] & (1 << ((i) % NBBY)))
#define	BitIsClear(a,i)		(!BitIsSet(a, i))

/* return -1 if no bits set */
static inline int BitFirstSet(void *a, u32 numBits)
{
	u32 numWords = (numBits + sizeof(u32) * NBBY - 1) / (sizeof(u32) * NBBY);
	for (u32 word = 0; word < numWords; word++) {
		u32 x = ((u32 *)a)[word];
		if (x) {
			int bit = bsf(x);
			bit += word * sizeof(u32) * NBBY;
			if (bit >= (int)numBits) {
				return -1;
			}
			return bit;
		}
	}
	return -1;
}

/* return -1 if no bits clear */
static inline int BitFirstClear(void *a, u32 numBits)
{
	u32 numWords = (numBits + sizeof(u32) * NBBY - 1) / (sizeof(u32) * NBBY) ;
	for (u32 word = 0; word < numWords; word++) {
		u32 x = ((u32 *)a)[word];
		if (x != (u32)~0) {
			int bit = bsf(~x);
			bit += word * sizeof(u32) * NBBY;
			if (bit >= (int)numBits) {
				return -1;
			}
			return bit;
		}
	}
	return -1;
}

#endif /* BITOPS_H_ */
