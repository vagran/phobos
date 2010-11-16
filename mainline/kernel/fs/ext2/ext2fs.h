/*
 * /phobos/kernel/fs/ext2/ext2fs.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#ifndef EXT2FS_H_
#define EXT2FS_H_
#include <sys.h>
phbSource("$Id$");

class Ext2FS : public DeviceFS {
public:

private:
	enum {
		MAGIC =					0xEF53,
		SUPERBLOCK_OFFSET =		1024,
		DIRECT_BLOCKS =			12,
		ROOT_DIR_ID =			2,
		OLD_REVISION =			0,
		OLD_INODE_SIZE =		128,
	};

	enum FSState {
		FSS_CLEAN =				0x1,
		FSS_ERROR =				0x2,
	};

	/* values in directory entry */
	enum FileType {
		FT_NONE,
		FT_REGULAR,
		FT_DIRECTORY,
		FT_CHARDEV,
		FT_BLOCKDEV,
		FT_PIPE,
		FT_SOCKET,
		FT_LINK,
		FT_MAX
	};

	enum InodeFileType {
		IFT_MASK =		0xf000,
		IFT_PIPE =		0x1000,
		IFT_CHARDEV =	0x2000,
		IFT_DIRECTORY =	0x4000,
		IFT_BLOCKDEV =	0x6000,
		IFT_REGULAR =	0x8000,
		IFT_LINK =		0xa000,
		IFT_SOCKET =	0xc000,
	};

	typedef struct {
		u32 total_inodes; /* Number of inodes in file system */
		u32 total_blocks; /* Number of blocks in file system */
		u32 reserved_blocks; /* Number of blocks reserved to prevent file system from filling up */
		u32 free_blocks; /* Number of unallocated blocks */
		u32 free_inodes; /* Number of unallocated inodes */
		u32 first_data_block; /* Block where block group 0 starts */
		u32 log2_block_size; /* Block size (saved as the number of places to shift 1024 to the left) */
		u32 log2_fragment_size; /* Fragment size (saved as the number of bits to shift 1024 to the left) */
		u32 blocks_per_group; /* Number of blocks in each block group */
		u32 fragments_per_group; /* Number of fragments in each block group */
		u32 inodes_per_group; /* Number of inodes in each block group */
		u32 mtime; /* Last mount time */
		u32 utime; /* Last written time */
		u16 mnt_count; /* Current mount count */
		u16 max_mnt_count; /* Maximum mount count */
		u16 magic; /* Signature */
		u16 fs_state; /* File system state */
		u16 error_handling; /* Error handling method */
		u16 minor_revision_level; /* Minor version */
		u32 lastcheck; /* Last consistency check time */
		u32 checkinterval; /* Interval between forced consistency checks */
		u32 creator_os; /* Creator OS */
		u32 revision_level; /* Major version */
		u16 uid_reserved; /* UID that can use reserved blocks */
		u16 gid_reserved; /* GID that can use reserved blocks */
		u32 first_inode; /* First non-reserved inode in file system */
		u16 inode_size; /* Size of each inode structure */
		u16 block_group_number; /* Block group that this superblock is part of (if backup copy) */
		u32 feature_compatibility; /* Compatible feature flags */
		u32 feature_incompat; /* Incompatible feature flags */
		u32 feature_ro_compat; /* Read only feature flags */
		u16 uuid[8]; /* File system ID */
		char volume_name[16]; /* Volume name */
		char last_mounted_on[64]; /* Path where last mounted on */
		u32 compression_info; /* Algorithm usage bitmap */
		u8 prealloc_blocks; /* Number of blocks to preallocate for files */
		u8 prealloc_dir_blocks; /* Number of blocks to preallocate for directories */
		u16 reserved_gdt_blocks;
		u8 journal_uuid[16]; /* Journal ID */
		u32 journal_inum; /* Journal inode */
		u32 journal_dev; /* Journal device */
		u32 last_orphan; /* Head of orphan inode list */
		u32 hash_seed[4];
		u8 def_hash_version;
		u8 jnl_backup_type;
		u16 reserved_word_pad;
		u32 default_mount_opts;
		u32 first_meta_bg;
		u32 mkfs_time;
		u32 jnl_blocks[17];
	} Superblock;

	typedef struct {
		u32 block_id;
		u32 inode_id;
		u32 inode_table_id;
		u16 free_blocks;
		u16 free_inodes;
		u16 used_dirs;
		u16 pad;
		u32 reserved[3];
	} BlocksGroup;

	typedef struct
	{
		u16 mode;
		u16 uid;
		u32 size;
		u32 atime;
		u32 ctime;
		u32 mtime;
		u32 dtime;
		u16 gid;
		u16 nlinks;
		u32 blockcnt; /* Blocks of 512 bytes!! */
		u32 flags;
		u32 osd1;
		union
		{
			struct {
				u32 dir_blocks[DIRECT_BLOCKS];
				u32 indir_block;
				u32 double_indir_block;
				u32 triple_indir_block;
			} blocks;
			char symlink[60];
		};
		u32 version;
		u32 acl;
		u32 dir_acl;
		u32 fragment_addr;
		u32 osd2[3];
	} Inode;

	typedef struct {
		u32 inode;
		u16 direntlen;
		u8 namelen;
		u8 filetype;
	} DirEntry;

	struct _Node;
	typedef struct _Node Node;
	struct _Node {
		ListEntry list;
		ListHead childs;
		u32 childNum;
		Node *parent;
		char *name; /* name in parent directory */
		u32 id; /* inode number */
		Inode *inode;
		BlocksGroup *group;
		u32 *indirBlock, *doubleIndirBlock, *tripleIndirBlock;
		u32 *lastBlockMap;
		u32 lastBlockMapOffset; /* block index */
		void *lastBlock;
		u32 lastBlockOffset;
		u32 indirBlockDirty:1,
			doubleIndirBlockDirty:1,
			tripleIndirBlockDirty:1,
			lastBlockMapDirty:1,
			lastBlockDirty:1,
			dirRead:1, /* directory is read and childs created */
			dirDirty:1;
	};

	Superblock sb;
	int sbDirty:1;
	Node *root;
	SpinLock treeLock;
	u32 blockSize;
	u32 numGroups;
	BlocksGroup *blocksGroups;
	/*
	 * Each element is pointer to two blocks, the first is blocks bitmap,
	 * the second - inodes bitmap.
	 */
	u8 **bitmaps;
	u8 *bgDirtyMap; /* block descriptor and bitmaps */
	Inode **inodeTable; /* element is pointer to inodes descriptors in one block of the table */
	u32 inodeTableSize;
	u32 inodeSize;
	u8 *inodeDirtyMap;

	Node *AllocateNode(Node *parent);
	Node *CreateNode(Node *parent, u32 id);
	void FreeNode(Node *node);
	Inode *GetInode(u32 id);
	u8 *GetBitmaps(u32 groupIdx);
	u8 *GetBlocksBitmap(u32 groupIdx);
	u8 *GetInodesBitmap(u32 groupIdx);
	int IsInodeAllocated(u32 id);
	void SetInodeAllocated(u32 id, int set = 1);
	inline BlocksGroup *GetGroup(u32 id);
	void SetFSError(const char *fmt,...);
	i32 ReadFile(Node *node, u32 offset, u32 len, void *buf);
	u32 GetBlockID(Node *node, u32 offset);
	int ReadDirectory(Node *node);
public:
	DeclareFSFactory();
	DeclareFSProber();

	Ext2FS(BlkDevice *dev, int flags);
	virtual ~Ext2FS();

	virtual Handle GetNode(Handle parent, const char *name, int nameLen = -1);
	virtual VFS::Node::Type GetNodeType(Handle node);
	virtual u64 GetNodeSize(Handle node);
	virtual i64 ReadNode(Handle node, off_t offset, u32 len, void *buf);
	virtual u32 ReadLink(Handle node, void *buf, u32 bufLen);
};

#endif /* EXT2FS_H_ */
