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

RegisterFS(Ext2FS, "ext2", "Second extended file system");

Ext2FS::Ext2FS(BlkDevice *dev) : DeviceFS(dev)
{
	if (dev->Read(SUPERBLOCK_OFFSET, &sb, sizeof(sb))) {
		klog(KLOG_WARNING, "Cannot read superblock: Device read error: %s%lu",
				dev->GetClass(), dev->GetUnit());
		return;
	}
	sbDirty = 1;
	blockSize = 1 << (sb.log2_block_size + 10);
	bgDirtyMap = 0;
	numGroups = (sb.total_inodes + sb.inodes_per_group - 1) / sb.inodes_per_group;
	blocksGroups = (BlocksGroup *)MM::malloc(sizeof(*blocksGroups) * numGroups);
	memset(blocksGroups, 0, sizeof(*blocksGroups) * numGroups);
	if (dev->Read((sb.first_data_block + 1) * blockSize, blocksGroups,
		sizeof(BlocksGroup) * numGroups)) {
		klog(KLOG_WARNING, "Cannot read blocks groups descriptors: Device read error: %s%lu",
				dev->GetClass(), dev->GetUnit());
		return;
	}
	bgDirtyMap = (u8 *)MM::malloc((numGroups + NBBY - 1) / NBBY);
	memset(bgDirtyMap, 0, (numGroups + NBBY - 1) / NBBY);
	status = 0;
}

Ext2FS::~Ext2FS()
{
	if (blocksGroups) {
		/* XXX should clean groups */
		MM::mfree(bgDirtyMap);
		MM::mfree(blocksGroups);
	}
}

DefineFSProber(Ext2FS)
{
	Superblock sb;

	if (dev->Read(SUPERBLOCK_OFFSET, &sb, sizeof(sb))) {
		klog(KLOG_WARNING, "Device read error: %s%lu",
			dev->GetClass(), dev->GetUnit());
		return -1;
	}
	return sb.magic == MAGIC ? 0 : -1;
}

Handle
Ext2FS::GetNode(Handle parent, const char *name, int nameLen)
{
	//notimpl
	return 0;
}

VFS::Node::Type
Ext2FS::GetNodeType(Handle node)
{
	//notimpl
	return VFS::Node::T_REGULAR;
}
