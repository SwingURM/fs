#include "img.h"

#include <cassert>
#include <cstring>

#include "InodeManager.h"
void ImgMaker::initFloppy(std::shared_ptr<MyDisk> bd) {
  auto sbm_ = std::make_shared<SuperBlockManager>(bd);
  auto bm_ = std::make_shared<BlockManager>(bd, sbm_, false);
  auto im_ = std::make_shared<InodeManager>(sbm_, bm_);
  SuperBlock sb;
  memset(&sb, 0, sizeof(SuperBlock));
  // Initialize superblock
  //   sb_.s_inodes_count_ = ? ;
  sb.s_blocks_count_ = 1440;
  // sb_.s_r_blocks_count_ = ;
  // sb_.s_free_blocks_count_ = 1439;
  // sb_.s_free_inodes_count_ = ;
  sb.s_first_data_block_ = 1;
  sb.s_log_block_size_ = 0;
  // sb_.s_log_frag_size_ = ;
  sb.s_blocks_per_group_ = 1439;
  // sb_.s_frags_per_group_ = ;
  sb.s_inodes_per_group_ = 23 * (1024 / sizeof(inode));
  sb.s_mtime_ = time(nullptr);
  sb.s_wtime_ = time(nullptr);
  // sb_.s_mnt_count_ = ;
  // sb_.s_max_mnt_count_ = ;
  sb.s_magic_ = EXT2_SUPER_MAGIC;
  // TODO 假设这里是挂载
  sb.s_state_ = EXT2_ERROR_FS;
  sbm_->writeSuperBlock(&sb);

  // Initialize block group descriptor table
  FSBlock blk{std::make_unique<char[]>(1024)};
  auto bgd = reinterpret_cast<Block_Group_Descriptor*>(blk.s_.get());
  bgd[0].bg_block_bitmap_ = 3;
  bgd[0].bg_inode_bitmap_ = 4;
  bgd[0].bg_inode_table_ = 5;
  bm_->writeBlock(blk, 2);
  bm_->refresh();

  // Initialize block bitmap
  memset(blk.s_.get(), 0, 1024);
  bm_->writeBlock(blk, 3);
  for (int i = 1; i < 28; i++) {
    // TODO: OPTIMIZE
    bm_->tagBlock(i, 1);
  }

  // Initialize inode bitmap
  bm_->writeBlock(blk, 4);
  // Initialize inode table
  bm_->writeBlock(blk, 5);

  inode root_inode;
  memset(&root_inode, 0, sizeof(inode));
  root_inode.i_mode_ = EXT2_S_IFDIR;
  root_inode.i_links_count_ = 2;

  //
  auto iid = im_->new_inode(root_inode);
  assert(iid == 1);

  im_->dir_add_dentry(1, 1, ".");
  im_->dir_add_dentry(1, 1, "..");
}

void ImgMaker::initFloppyPlus(std::shared_ptr<MyDisk> bd) {
  auto sbm_ = std::make_shared<SuperBlockManager>(bd);
  auto bm_ = std::make_shared<BlockManager>(bd, sbm_, false);
  auto im_ = std::make_shared<InodeManager>(sbm_, bm_);
  SuperBlock sb;
  memset(&sb, 0, sizeof(SuperBlock));
  // Initialize superblock
  //   sb_.s_inodes_count_ = ? ;
  sb.s_blocks_count_ = 1440;
  // sb_.s_r_blocks_count_ = ;
  // sb_.s_free_blocks_count_ = 1439;
  // sb_.s_free_inodes_count_ = ;
  sb.s_first_data_block_ = 0;
  sb.s_log_block_size_ = 2;
  // sb_.s_log_frag_size_ = ;
  sb.s_blocks_per_group_ = 1439;
  // sb_.s_frags_per_group_ = ;
  sb.s_inodes_per_group_ = 23 * (4096 / sizeof(inode));
  sb.s_mtime_ = time(nullptr);
  sb.s_wtime_ = time(nullptr);
  // sb_.s_mnt_count_ = ;
  // sb_.s_max_mnt_count_ = ;
  sb.s_magic_ = EXT2_SUPER_MAGIC;
  // TODO 假设这里是挂载
  sb.s_state_ = EXT2_ERROR_FS;
  sbm_->writeSuperBlock(&sb);

  // Initialize block group descriptor table
  FSBlock blk{std::make_unique<char[]>(4096)};
  auto bgd = reinterpret_cast<Block_Group_Descriptor*>(blk.s_.get());
  bgd[0].bg_block_bitmap_ = 2;
  bgd[0].bg_inode_bitmap_ = 3;
  bgd[0].bg_inode_table_ = 4;
  bm_->writeBlock(blk, 1);
  bm_->refresh();

  // Initialize block bitmap
  memset(blk.s_.get(), 0, 4096);
  bm_->writeBlock(blk, 2);
  for (int i = 0; i < 28; i++) {
    // TODO: OPTIMIZE
    bm_->tagBlock(i, 1);
  }

  // Initialize inode bitmap
  bm_->writeBlock(blk, 3);
  // Initialize inode table
  bm_->writeBlock(blk, 4);

  inode root_inode;
  memset(&root_inode, 0, sizeof(inode));
  root_inode.i_mode_ = EXT2_S_IFDIR;
  root_inode.i_links_count_ = 2;

  //
  auto iid = im_->new_inode(root_inode);
  assert(iid == 1);

  im_->dir_add_dentry(1, 1, ".");
  im_->dir_add_dentry(1, 1, "..");
}