/*
 * /phobos/kernel/fs/ext2/ext2fs.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef EXT2FS_H_
#define EXT2FS_H_
#include <sys.h>
phbSource("$Id$");

class Ext2FS : public DeviceFS {
public:

private:

public:
	Ext2FS(BlkDevice *dev);
	virtual ~Ext2FS();


};

#endif /* EXT2FS_H_ */
