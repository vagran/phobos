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

Ext2FS::Ext2FS(BlkDevice *dev, int flags) : DeviceFS(dev, flags)
{
	root = 0;
	blocksGroups = 0;
	bgDirtyMap = 0;
	inodeTable = 0;

	if (dev->Read(SUPERBLOCK_OFFSET, &sb, sizeof(sb))) {
		klog(KLOG_WARNING, "Cannot read superblock: Device read error: %s%lu",
				dev->GetClass(), dev->GetUnit());
		return;
	}
	if (flags & F_READONLY) {
		sbDirty = 0;
	} else {
		sb.fs_state &= ~FSS_CLEAN;
		sbDirty = 1;
	}
	if (sb.revision_level == OLD_REVISION) {
		inodeSize = OLD_INODE_SIZE;
	} else {
		inodeSize = sb.inode_size;
	}

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
	bitmaps = (u8 **)MM::malloc(numGroups * sizeof(u8 *));
	memset(bitmaps, 0, numGroups * sizeof(u8 *));

	bgDirtyMap = (u8 *)MM::malloc((numGroups + NBBY - 1) / NBBY);
	memset(bgDirtyMap, 0, (numGroups + NBBY - 1) / NBBY);

	inodeTableSize = (sb.total_inodes * inodeSize + blockSize - 1) / blockSize;
	inodeTable = (Inode **)MM::malloc(inodeTableSize * sizeof (Inode *));
	memset(inodeTable, 0, inodeTableSize * sizeof (Inode *));
	inodeDirtyMap = (u8 *)MM::malloc((sb.total_inodes + NBBY - 1) / NBBY);
	memset(inodeDirtyMap, 0, (sb.total_inodes + NBBY - 1) / NBBY);

	/* read root directory */
	root = CreateNode(0, ROOT_DIR_ID);
	if (!root) {
		klog(KLOG_WARNING, "Cannot create root directory: %s%lu",
			dev->GetClass(), dev->GetUnit());
		return;
	}
	if ((root->inode->mode & IFT_MASK) != IFT_DIRECTORY) {
		klog(KLOG_WARNING, "Root directory inode has non-directory type: %s%lu",
			dev->GetClass(), dev->GetUnit());
		return;
	}
	status = 0;
}

Ext2FS::~Ext2FS()
{
	if (root) {
		treeLock.Lock();
		FreeNode(root);
		root = 0;
		treeLock.Unlock();
	}
	if (inodeTable) {
		/* XXX should clean inodes descriptors */
		for (u32 i = 0; i < inodeTableSize; i++) {
			if (inodeTable[i]) {
				MM::mfree(inodeTable[i]);
			}
		}
		MM::mfree(inodeTable);
	}
	if (inodeDirtyMap) {
		MM::mfree(inodeDirtyMap);
	}
	if (blocksGroups) {
		/* XXX should clean groups */
		MM::mfree(bgDirtyMap);
		MM::mfree(blocksGroups);
	}
	if (bitmaps) {
		for (u32 i = 0; i < numGroups; i++) {
			if (bitmaps[i]) {
				MM::mfree(bitmaps[i]);
			}
		}
		MM::mfree(bitmaps);
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

/* must be called with treeLock */
void
Ext2FS::FreeNode(Node *node)
{
	Node *childNode;
	while ((childNode = LIST_FIRST(Node, list, node->childs))) {
		FreeNode(childNode);
	}
	/* XXX should clean node */
	if (node->parent) {
		LIST_DELETE(list, node, node->parent->childs);
		node->parent->childNum--;
	}
	DELETE(node);
}

Ext2FS::Node *
Ext2FS::AllocateNode(Node *parent)
{
	Node *node = NEW(Node);
	if (!node) {
		return 0;
	}
	memset(node, 0, sizeof(*node));
	node->parent = parent;
	LIST_INIT(node->childs);
	if (parent) {
		treeLock.Lock();
		LIST_ADD(list, node, parent->childs);
		parent->childNum++;
		treeLock.Unlock();
	}
	return node;
}

u8 *
Ext2FS::GetBitmaps(u32 groupIdx)
{
	u8 **bitmap = &bitmaps[groupIdx];
	if (*bitmap) {
		return *bitmap;
	}
	*bitmap = (u8 *)MM::malloc(2 * blockSize);
	if (!*bitmap) {
		return 0;
	}
	BlocksGroup *group = &blocksGroups[groupIdx];
	if (dev->Read(((u64)group->block_id) * blockSize, *bitmap, blockSize)) {
		MM::mfree(*bitmap);
		*bitmap = 0;
		return 0;
	}
	if (dev->Read(((u64)group->inode_id) * blockSize,
		(*bitmap) + blockSize, blockSize)) {
		MM::mfree(*bitmap);
		*bitmap = 0;
		return 0;
	}
	return *bitmap;
}

u8 *
Ext2FS::GetBlocksBitmap(u32 groupIdx)
{
	return GetBitmaps(groupIdx);
}

u8 *
Ext2FS::GetInodesBitmap(u32 groupIdx)
{
	u8 *b = GetBitmaps(groupIdx);
	if (!b) {
		return 0;
	}
	return b + blockSize;
}

Ext2FS::BlocksGroup *
Ext2FS::GetGroup(u32 id)
{
	return &blocksGroups[(id - 1) / sb.inodes_per_group];
}

Ext2FS::Inode *
Ext2FS::GetInode(u32 id)
{
	BlocksGroup *group = GetGroup(id);
	id--;
	u32 inodesPerBlock = blockSize / inodeSize;
	Inode **inodeBlock = &inodeTable[id / inodesPerBlock];
	if (!*inodeBlock) {
		*inodeBlock = (Inode *)MM::malloc(blockSize);
		if (!*inodeBlock) {
			return 0;
		}
		u32 blockIdx = (id % sb.inodes_per_group) / inodesPerBlock;
		if (dev->Read(((u64)(group->inode_table_id + blockIdx)) * blockSize,
			*inodeBlock, blockSize)) {
			MM::mfree(*inodeBlock);
			*inodeBlock = 0;
			return 0;
		}
	}
	return &(*inodeBlock)[id % inodesPerBlock];
}

int
Ext2FS::IsInodeAllocated(u32 id)
{
	id--;
	u32 groupIdx = id / sb.inodes_per_group;
	u8 *bitmap = GetInodesBitmap(groupIdx);
	return BitIsSet(bitmap, id % sb.inodes_per_group);
}

void
Ext2FS::SetInodeAllocated(u32 id, int set)
{
	id--;
	u32 groupIdx = id / sb.inodes_per_group;
	u8 *bitmap = GetInodesBitmap(groupIdx);
	if (set) {
		BitSet(bitmap, id % sb.inodes_per_group);
	} else {
		BitClear(bitmap, id % sb.inodes_per_group);
	}
}

Ext2FS::Node *
Ext2FS::CreateNode(Node *parent, u32 id)
{
	if (id > sb.total_inodes) {
		return 0;
	}
	int isNew = !IsInodeAllocated(id);
	Inode *inode = GetInode(id);
	if (!inode) {
		return 0;
	}
	Node *node = AllocateNode(parent);
	if (!node) {
		return 0;
	}
	node->id = id;
	node->group = GetGroup(id);
	node->inode = inode;
	if (isNew) {
		memset(inode, 0, sizeof(Inode));
		SetInodeAllocated(id);
		if (!node->group->free_inodes) {
			node->group->free_inodes--;
		} else {
			SetFSError("Inconsistent free inodes counter in group");
		}
		if (!sb.free_inodes) {
			sb.free_inodes--;
		} else {
			SetFSError("Inconsistent free inodes counter in superblock");
		}
	}
	return node;
}

void
Ext2FS::SetFSError(const char *fmt,...)
{
	sb.fs_state |= FSS_ERROR;
	sbDirty = 1;
	va_list va;
	va_start(va, fmt);
	klogv(KLOG_ERROR, fmt, va);
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
