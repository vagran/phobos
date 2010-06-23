/*
 * /phobos/kernel/sys/bitmap.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

class Bitmap : public Object {
private:
	int firstSet, lastUsed;
	int numBits;
	u8 *map;
	int numAllocated;
public:
	Bitmap(u32 size);
	~Bitmap();

	int AllocateBit();
	int ReleaseBit(int bitIdx);
};
