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

DefineFSFactory(Ext2FS);

RegisterFS(Ext2FS, "ext2", "Second extended filesystem");

Ext2FS::Ext2FS(BlkDevice *dev) : DeviceFS(dev)
{

}

Ext2FS::~Ext2FS()
{

}

DefineFSProber(Ext2FS)
{
	Superblock sb;

	if (dev->Read(SUPERBLOCK_OFFSET, &sb, sizeof(sb))) {
		klog(KLOG_WARNING, "Device read error: %s%lu",
			devMan.GetClass(dev->GetClassID()), dev->GetUnit());
		return -1;
	}
	return sb.magic == MAGIC ? 0 : -1;
}
