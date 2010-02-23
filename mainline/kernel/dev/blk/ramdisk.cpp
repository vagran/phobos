/*
 * /phobos/kernel/dev/blk/ramdisk.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include "ramdisk.h"

DefineDevFactory(Ramdisk);

RegProbeDevClass(Ramdisk, "ramdisk", Device::T_BLOCK, "RAM disk");

extern u32 _ramdiskSize;
extern u8 _ramdisk[];

DefineDevProber(Ramdisk)
{
	/* we have unit 0 when .ramdisk section is filled */
	if (lastUnit == -1 && _ramdiskSize) {
		return 0;
	}
	return -1;
}

Ramdisk::Ramdisk(Type type, u32 unit, u32 classID) :
	BlkDevice(type, unit, classID)
{
	assert(unit == 0 && _ramdiskSize);
	blockSize = BLOCK_SIZE;
	size = _ramdiskSize / BLOCK_SIZE;
	data = _ramdisk;

	devState = S_UP;
}

int
Ramdisk::Push(IOBuf *buf)
{
	if (AcceptBuffer(buf)) {
		return -1;
	}
	if (buf->addr + buf->size >= GetSize()) {
		CompleteBuffer(buf, IOBuf::S_OUT_OF_RANGE);
		return 0;
	}
	u8 *loc = data + buf->addr;
	if (buf->GetDirection() == IOBuf::F_DIR_IN) {
		memcpy(buf->buf, loc, buf->size);
	} else {
		memcpy(loc, buf->buf, buf->size);
	}
	CompleteBuffer(buf);
	return 0;
}

int
Ramdisk::Pull(IOBuf *buf)
{
	return ReleaseBuffer(buf);
}
