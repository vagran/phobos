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
	if (node->lastBlock) {
		MM::mfree(node->lastBlock);
	}
	if (node->indirBlock) {
		MM::mfree(node->indirBlock);
	}
	if (node->doubleIndirBlock) {
		MM::mfree(node->doubleIndirBlock);
	}
	if (node->tripleIndirBlock) {
		MM::mfree(node->tripleIndirBlock);
	}
	if (node->lastBlockMap) {
		MM::mfree(node->lastBlockMap);
	}
	if (node->parent) {
		LIST_DELETE(list, node, node->parent->childs);
		node->parent->childNum--;
	}
	if (node->name) {
		MM::mfree(node->name);
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

u32
Ext2FS::GetBlockID(Node *node, u32 offset)
{
	if (offset >= roundup2(node->inode->size, blockSize)) {
		return 0;
	}
	u32 blockIndex = offset / blockSize;
	if (blockIndex < DIRECT_BLOCKS) {
		/* use direct blocks */
		return node->inode->blocks.dir_blocks[blockIndex];
	}

	u32 idsPerBlock = blockSize / sizeof(u32);
	if (blockIndex < DIRECT_BLOCKS + idsPerBlock) {
		/* use indirect blocks */
		if (!node->indirBlock) {
			if (!node->inode->blocks.indir_block) {
				return 0;
			}
			node->indirBlock = (u32 *)MM::malloc(blockSize);
			if (!node->indirBlock) {
				return 0;
			}
			if (dev->Read(node->inode->blocks.indir_block * blockSize,
				node->indirBlock, blockSize)) {
				MM::mfree(node->indirBlock);
				node->indirBlock = 0;
				return 0;
			}
		}
		return node->indirBlock[blockIndex - DIRECT_BLOCKS];
	}
	/* check blocks map cache */
	if (node->lastBlockMap && (blockIndex >= node->lastBlockMapOffset &&
		blockIndex < node->lastBlockMapOffset + idsPerBlock)) {
		return node->lastBlockMap[blockIndex - node->lastBlockMapOffset];
	}
	if (blockIndex < DIRECT_BLOCKS + idsPerBlock * (1 + idsPerBlock)) {
		/* use double indirect blocks */
		if (!node->doubleIndirBlock) {
			if (!node->inode->blocks.double_indir_block) {
				return 0;
			}
			node->doubleIndirBlock = (u32 *)MM::malloc(blockSize);
			if (!node->doubleIndirBlock) {
				return 0;
			}
			if (dev->Read(node->inode->blocks.double_indir_block * blockSize,
				node->doubleIndirBlock, blockSize)) {
				MM::mfree(node->doubleIndirBlock);
				node->doubleIndirBlock = 0;
				return 0;
			}
		}
		if (node->lastBlockMap) {
			/* XXX should clean */
		} else {
			node->lastBlockMap = (u32 *)MM::malloc(blockSize);
			if (!node->lastBlockMap) {
				return 0;
			}
		}
		u32 doubleIdx = (blockIndex - DIRECT_BLOCKS - idsPerBlock) / idsPerBlock;
		if (dev->Read(node->doubleIndirBlock[doubleIdx] * blockSize,
			node->lastBlockMap, blockSize)) {
			MM::mfree(node->lastBlockMap);
			node->lastBlockMap = 0;
			return 0;
		}
		node->lastBlockMapOffset = DIRECT_BLOCKS + idsPerBlock +
			doubleIdx * idsPerBlock;
	} else if (blockIndex < DIRECT_BLOCKS + idsPerBlock * (1 +
		idsPerBlock * (1 + idsPerBlock))) {
		u32 prevOffset = DIRECT_BLOCKS + idsPerBlock * (1 + idsPerBlock);
		/* use triple indirect blocks */
		if (!node->tripleIndirBlock) {
			if (!node->inode->blocks.triple_indir_block) {
				return 0;
			}
			node->tripleIndirBlock = (u32 *)MM::malloc(blockSize);
			if (!node->tripleIndirBlock) {
				return 0;
			}
			if (dev->Read(node->inode->blocks.triple_indir_block * blockSize,
				node->tripleIndirBlock, blockSize)) {
				MM::mfree(node->tripleIndirBlock);
				node->tripleIndirBlock = 0;
				return 0;
			}
		}
		u32 tripleIdx = (blockIndex - prevOffset) / (idsPerBlock * idsPerBlock);
		u32 doubleMap[blockSize];
		if (dev->Read(node->tripleIndirBlock[tripleIdx] * blockSize,
			doubleMap, blockSize)) {
			return 0;
		}
		if (node->lastBlockMap) {
			/* XXX should clean */
		} else {
			node->lastBlockMap = (u32 *)MM::malloc(blockSize);
			if (!node->lastBlockMap) {
				return 0;
			}
		}
		u32 doubleOffset = prevOffset + tripleIdx * idsPerBlock * idsPerBlock;
		u32 doubleIdx = (blockIndex - doubleOffset) / idsPerBlock;
		if (dev->Read(doubleMap[doubleIdx] * blockSize,
			node->lastBlockMap, blockSize)) {
			MM::mfree(node->lastBlockMap);
			node->lastBlockMap = 0;
			return 0;
		}
		node->lastBlockMapOffset = doubleOffset + doubleIdx * idsPerBlock;
	} else {
		return 0;
	}
	/* get from cached map */
	assert(node->lastBlockMap);
	return node->lastBlockMap[blockIndex - node->lastBlockMapOffset];
}

u64
Ext2FS::GetNodeSize(Handle node)
{
	return ((Node *)node)->inode->size;
}

u32
Ext2FS::ReadLink(Handle node, void *buf, u32 bufLen)
{
	u32 size = ((Node *)node)->inode->size;
	u32 count = min(size, bufLen);
	if (size <= FIELDSIZE(Inode, symlink)) {
		memcpy(buf, ((Node *)node)->inode->symlink, count);
	} else {
		u32 numRead = ReadFile((Node *)node, 0, count, buf);
		if (numRead != count) {
			return 0;
		}
	}
	if (count < bufLen) {
		((char *)buf)[count] = 0;
		count++;
	}
	return count;
}

i64
Ext2FS::ReadNode(Handle node, u64 offset, u32 len, void *buf)
{
	return ReadFile((Node *)node, offset, len, buf);
}

i32
Ext2FS::ReadFile(Node *node, u32 offset, u32 len, void *buf)
{
	u32 count = 0;
	u32 size = node->inode->size;
	if (offset >= size) {
		return 0;
	}
	if (!node->lastBlock) {
		node->lastBlock = MM::malloc(blockSize);
		if (!node->lastBlock) {
			return 0;
		}
		node->lastBlockOffset = ~0ul;
	}
	while (len && offset < size) {
		void *pBuf;
		u32 readOffset;
		u32 inOffset, inLen;
		if (offset & (blockSize - 1)) {
			pBuf = node->lastBlock;
			readOffset = rounddown2(offset, blockSize);
			inOffset = offset - readOffset;
			inLen = min(blockSize - inOffset, len);
		} else {
			readOffset = offset;
			inOffset = 0;
			if (len >= blockSize) {
				pBuf = buf;
				inLen = blockSize;
			} else {
				pBuf = node->lastBlock;
				inLen = len;
			}
		}
		inLen = min(inLen, size - offset);
		u32 blockId = GetBlockID(node, readOffset);
		if (!blockId) {
			/* hole in the file, read zeros */
			memset(buf, 0, inLen);
		} else {
			if (pBuf != node->lastBlock || readOffset != node->lastBlockOffset) {
				if (dev->Read((u64)blockId * blockSize, pBuf, blockSize)) {
					if (pBuf == node->lastBlock) {
						MM::mfree(node->lastBlock);
						node->lastBlock = 0;
					}
					if (node->lastBlockOffset == ~0ul) {
						MM::mfree(node->lastBlock);
						node->lastBlock = 0;
					}
					return count;
				}
				if (pBuf == node->lastBlock) {
					node->lastBlockOffset = readOffset;
				}
			}
			if (pBuf != buf) {
				memcpy(buf, ((u8 *)pBuf) + inOffset, inLen);
			}
		}
		buf = ((u8 *)buf) + inLen;
		count += inLen;
		offset += inLen;
		len -= inLen;
	}
	if (node->lastBlockOffset == ~0ul) {
		MM::mfree(node->lastBlock);
		node->lastBlock = 0;
	}
	return count;
}

int
Ext2FS::ReadDirectory(Node *node)
{
	if (node->dirRead) {
		return 0;
	}
	u32 pos = 0;
	while (pos < node->inode->size) {
		DirEntry dirEnt;
		if (ReadFile(node, pos, sizeof(dirEnt), &dirEnt) != sizeof(dirEnt)) {
			break;
		}
		if (dirEnt.inode) {
			if (!dirEnt.direntlen) {
				break;
			}
			if (dirEnt.namelen) {
				char filename[dirEnt.namelen + 1];
				if (ReadFile(node, pos + sizeof(dirEnt), dirEnt.namelen, filename) !=
					dirEnt.namelen) {
					break;
				}
				filename[dirEnt.namelen] = 0;
				/* do not create entries for current and parent directories */
				if (strcmp(filename, ".") && strcmp(filename, "..")) {
					Node *child = CreateNode(node, dirEnt.inode);
					if (!child) {
						break;
					}
					child->name = strdup(filename);
				}
			}
		}
		if (!dirEnt.direntlen) {
			break;
		}
		pos += dirEnt.direntlen;
	}
	node->dirRead = 1;
	return 0;
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
	Node *node;
	if (!parent) {
		if (!name) {
			return root;
		}
		node = root;
	} else {
		node = (Node *)parent;
	}
	if ((node->inode->mode & IFT_MASK) != IFT_DIRECTORY) {
		klog(KLOG_WARNING, "Not a directory node specified as parent");
		return 0;
	}
	if (ReadDirectory(node)) {
		klog(KLOG_WARNING, "Cannot read directory");
		return 0;
	}
	Node *p;
	LIST_FOREACH(Node, list, p, node->childs) {
		if (!p->name) {
			continue;
		}
		if (nameLen == -1) {
			if (!strcmp(name, p->name)) {
				return p;
			}
		} else {
			if (!strncmp(name, p->name, nameLen) && !p->name[nameLen]) {
				return p;
			}
		}
	}
	return 0;
}

VFS::Node::Type
Ext2FS::GetNodeType(Handle node)
{
	switch (((Node *)node)->inode->mode & IFT_MASK) {
	case IFT_REGULAR:
		return VFS::Node::T_REGULAR;
	case IFT_DIRECTORY:
		return VFS::Node::T_DIRECTORY;
	case IFT_SOCKET:
		return VFS::Node::T_SOCKET;
	case IFT_PIPE:
		return VFS::Node::T_PIPE;
	case IFT_CHARDEV:
		return VFS::Node::T_CHARDEV;
	case IFT_BLOCKDEV:
		return VFS::Node::T_BLOCKDEV;
	case IFT_LINK:
		return VFS::Node::T_LINK;
	}
	return VFS::Node::T_NONE;
}
