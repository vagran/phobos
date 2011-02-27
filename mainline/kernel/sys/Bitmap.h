/*
 * /phobos/kernel/sys/bitmap.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
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
