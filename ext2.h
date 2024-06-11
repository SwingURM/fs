#include <stdint.h>

#include "device.h"

// Sample Floppy Disk Layout, 1KiB blocks

// Block       Offset        Length Description
// byte 0      512 bytes     boot record(if present)
// byte 512    512 bytes     additional boot record data(if present)
// -- block group 0, blocks 1 to 1439 --
// byte 1024   1024 bytes    superblock
// block 2     1 block       block group descriptor table
// block 3     1 block       block bitmap
// block 4     1 block       inode bitmap
// block 5     23 blocks     inode table
// block 28    1412 blocks   data blocks

struct SuperBlock {
  uint32_t s_inodes_count_;       // Inodes count
  uint32_t s_blocks_count_;       // Blocks count
  uint32_t s_r_blocks_count_;     // Reserved blocks count
  uint32_t s_free_blocks_count_;  // Free blocks count
  uint32_t s_free_inodes_count_;  // Free inodes count
  uint32_t s_first_data_block_;   // First Data Block
  uint32_t s_log_block_size_;     // Block size
  uint32_t s_log_frag_size_;      // Fragment size
  uint32_t s_blocks_per_group_;   // # Blocks per group
  uint32_t s_frags_per_group_;    // # Fragments per group
  uint32_t s_inodes_per_group_;   // # Inodes per group
  uint32_t s_mtime_;              // Mount time
  uint32_t s_wtime_;              // Write time
  uint16_t s_mnt_count_;          // Mount count
  uint16_t s_max_mnt_count_;      // Maximal mount count
  uint16_t s_magic_;              // Magic signature
  uint16_t s_state_;              // File system state
  uint16_t s_errors_;             // Behaviour when detecting errors
  uint16_t s_minor_rev_level_;    // minor revision level
  uint32_t s_lastcheck_;          // time of last check
  uint32_t s_checkinterval_;      // max. time between checks
  uint32_t s_creator_os_;         // OS
  uint32_t s_rev_level_;          // Revision level
  uint16_t s_def_resuid_;         // Default uid for reserved blocks
  uint16_t s_def_resgid_;         // Default gid for reserved blocks
  // These fields are for EXT2_DYNAMIC_REV superblocks only.
  // TODO
};

struct Block_Group_Descriptor {
  uint32_t bg_block_bitmap_;       // Blocks bitmap block
  uint32_t bg_inode_bitmap_;       // Inodes bitmap block
  uint32_t bg_inode_table_;        // Inodes table block
  uint16_t bg_free_blocks_count_;  // Free blocks count
  uint16_t bg_free_inodes_count_;  // Free inodes count
  uint16_t bg_used_dirs_count_;    // Directories count
  uint16_t bg_pad_;
  uint32_t bg_reserved_[3];
};

struct Block_Group_Descriptor_Table {
  Block_Group_Descriptor bg_desc_[BLOCK_SIZE / sizeof(Block_Group_Descriptor)];
};

struct inode {
  uint16_t i_mode_;         // File mode
  uint16_t i_uid_;          // Low 16 bits of Owner Uid
  uint32_t i_size_;         // Size in bytes
  uint32_t i_atime_;        // Access time
  uint32_t i_ctime_;        // Creation time
  uint32_t i_mtime_;        // Modification time
  uint32_t i_dtime_;        // Deletion Time
  uint16_t i_gid_;          // Low 16 bits of Group Id
  uint16_t i_links_count_;  // Links count
  uint32_t i_blocks_;       // Blocks count
  uint32_t i_flags_;        // File flags
  uint32_t i_osd1_;         // OS dependent 1
  uint32_t i_block_[15];    // Pointers to blocks
  uint32_t i_generation_;   // File version (for NFS)
  uint32_t i_file_acl_;     // File ACL
  uint32_t i_dir_acl_;      // Directory ACL
  uint32_t i_faddr_;        // Fragment address
  uint32_t i_osd2_[3];      // OS dependent 2
};

struct inode_table {
  inode inodes_[BLOCK_SIZE / sizeof(inode)];
};

struct dentry {
  uint32_t inode_;
  uint16_t rec_len_;
  uint8_t name_len_;
  uint8_t file_type_;
};

#define EXT2_SUPER_MAGIC (0xEF53)

#define EXT2_ERROR_FS (1)
#define EXT2_VALID_FS (2)

#define EXT2_S_IFREG (0x8000)
#define EXT2_S_IFDIR (0x4000)
#define EXT2_S_IFLNK (0xA000)
#define EXT2_S_IRUSR (0x0100)
#define EXT2_S_IWUSR (0x0080)

// my

#define NDIRECT_BLOCK (12)
#define N1INDIRECT_BLOCK (1)
#define N2INDIRECT_BLOCK (1)
#define N3INDIRECT_BLOCK (1)

#define UPPER4(x) (((x) + 3) / 4 * 4)

// dentry
#define EXT2_FT_UNKNOWN (0)
#define EXT2_FT_REG_FILE (1)
#define EXT2_FT_DIR (2)
#define EXT2_FT_SYMLINK (7)