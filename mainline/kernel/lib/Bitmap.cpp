/*
 * /phobos/kernel/lib/common/Bitmap.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

Bitmap::Bitmap(u32 size)
{
	firstSet = -1;
	lastUsed = -1;
	numAllocated = 0;
	assert(size);
	numBits = size;
	int numBytes = (size + NBBY - 1) / NBBY;
	map = (u8 *)MM::malloc(numBytes);
	ensure(map);
	memset(map, 0, numBytes);
}

Bitmap::~Bitmap()
{
	MM::mfree(map);
}

int
Bitmap::AllocateBit()
{
	int newIdx = lastUsed + 1;
	if (newIdx >= numBits) {
		newIdx = 0;
	}
	if (firstSet == -1) {
		firstSet = newIdx;
		lastUsed = newIdx;
		BitSet(map, newIdx);
		numAllocated++;
		return newIdx;
	}
	if (newIdx != firstSet) {
		lastUsed = newIdx;
		BitSet(map, newIdx);
		numAllocated++;
		return newIdx;
	}
	int nextIdx = newIdx + 1;
	do {
		if (nextIdx >= numBits) {
			nextIdx = 0;
		}
		if (BitIsClear(map, nextIdx)) {
			newIdx = nextIdx;
			lastUsed = newIdx;
			BitSet(map, newIdx);
			nextIdx++;
			if (nextIdx >= numBits) {
				nextIdx = 0;
			}
			/* advance firstSet pointer */
			while (BitIsClear(map, nextIdx)) {
				nextIdx++;
				if (nextIdx >= numBits) {
					nextIdx = 0;
				}
			}
			firstSet = nextIdx;
			numAllocated++;
			return newIdx;
		}
	} while (nextIdx != newIdx);
	/* nothing found */
	return -1;
}

int
Bitmap::ReleaseBit(int bitIdx)
{
	assert(BitIsSet(map, bitIdx));
	BitClear(map, bitIdx);
	if (bitIdx == lastUsed) {
		lastUsed--;
		if (lastUsed < 0) {
			lastUsed = numBits - 1;
		}
	}
	if (bitIdx == firstSet) {
		/* find next set */
		int nextIdx = bitIdx + 1;
		do {
			if (nextIdx >= numBits) {
				nextIdx = 0;
			}
			if (BitIsSet(map, nextIdx)) {
				firstSet = nextIdx;
				return 0;
			}
		} while (nextIdx != bitIdx);
		/* no set bits found */
		firstSet = -1;
	}
	numAllocated--;
	return 0;
}
