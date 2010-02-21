/*
 * /phobos/kernel/fs/fs.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef FS_H_
#define FS_H_
#include <sys.h>
phbSource("$Id$");

/* Device filesystem */

class DeviceFS {
public:

protected:
	BlkDevice *dev;

public:
	DeviceFS(BlkDevice *dev);
	virtual ~DeviceFS();
};

#endif /* FS_H_ */
