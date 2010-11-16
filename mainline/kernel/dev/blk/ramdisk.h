/*
 * /phobos/kernel/dev/blk/ramdisk.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#ifndef RAMDISK_H_
#define RAMDISK_H_
#include <sys.h>
phbSource("$Id$");

class Ramdisk : public BlkDevice {
public:
	enum {
		BLOCK_SIZE =		512,
	};
private:
	u8 *data;
public:
	DeclareDevFactory();
	DeclareDevProber();
	Ramdisk(Type type, u32 unit, u32 classID);

	virtual int Push(IOBuf *buf);
	virtual int Pull(IOBuf *buf);
};

#endif /* RAMDISK_H_ */
