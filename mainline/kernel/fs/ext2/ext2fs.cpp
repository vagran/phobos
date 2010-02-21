/*
 * /phobos/kernel/fs/ext2/ext2fs.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <fs/ext2/ext2fs.h>

Ext2FS::Ext2FS(BlkDevice *dev) : DeviceFS(dev)
{

}

Ext2FS::~Ext2FS()
{

}
