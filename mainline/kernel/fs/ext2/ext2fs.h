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
	enum {
		MAGIC =					0xEF53,
		SUPERBLOCK_OFFSET =		1024,
	};

	typedef struct
	{
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
public:
	DeclareFSFactory();
	DeclareFSProber();

	Ext2FS(BlkDevice *dev);
	virtual ~Ext2FS();
};

#endif /* EXT2FS_H_ */
