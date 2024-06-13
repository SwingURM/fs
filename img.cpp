#include "img.h"

#include <cassert>
#include <cstring>

#include "InodeManager.h"
#include "util.h"
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
  root_inode.i_mode_ = EXT2_S_IFDIR | 777;
  root_inode.i_links_count_ = 2;

  //
  auto iid = im_->new_inode(root_inode);
  assert(iid == 1);

  im_->dir_add_dentry(1, 1, ".", EXT2_FT_DIR);
  im_->dir_add_dentry(1, 1, "..", EXT2_FT_DIR);
}

void ImgMaker::initFloppyPlus(std::shared_ptr<MyDisk> bd) {
  auto sbm_ = std::make_shared<SuperBlockManager>(bd);
  auto bm_ = std::make_shared<BlockManager>(bd, sbm_, false);
  auto im_ = std::make_shared<InodeManager>(sbm_, bm_);
  SuperBlock sb;
  memset(&sb, 0, sizeof(SuperBlock));
  // Initialize superblock
  //   sb_.s_inodes_count_ = ? ;
  sb.s_blocks_count_ = 24577;
  // sb_.s_r_blocks_count_ = ;
  // sb_.s_free_blocks_count_ = 1439;
  // sb_.s_free_inodes_count_ = ;
  sb.s_first_data_block_ = 1;
  sb.s_log_block_size_ = 0;
  // sb_.s_log_frag_size_ = ;
  sb.s_blocks_per_group_ = 8192;
  // sb_.s_frags_per_group_ = ;
  sb.s_inodes_per_group_ = 214 * (1024 / sizeof(inode));
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
  bgd[1].bg_block_bitmap_ = 8195;
  bgd[1].bg_inode_bitmap_ = 8196;
  bgd[1].bg_inode_table_ = 8197;
  bgd[2].bg_block_bitmap_ = 16385;
  bgd[2].bg_inode_bitmap_ = 16386;
  bgd[2].bg_inode_table_ = 16387;
  bm_->writeBlock(blk, 2);
  bm_->refresh();

  // Initialize block bitmap
  // Initialize inode bitmap
  // Initialize inode table
  memset(blk.s_.get(), 0, 1024);
  for (int group = 0; group < bm_->bgd_.size(); group++) {
    bm_->writeBlock(blk, bm_->bgd_[group].bg_block_bitmap_);
    bm_->writeBlock(blk, bm_->bgd_[group].bg_inode_bitmap_);
    // TODO: OPTIMIZE
    // TODO: ASSERT 1 block bitmap ,inode tbl place just before data block
    for (int i = 0; i < 214; i++) {
      bm_->writeBlock(blk, bm_->bgd_[group].bg_inode_table_ + i);
    }
  }
  // dont forget tag superblock and bgd
  for (int group = 0; group < bm_->bgd_.size(); group++) {
    if (group == 0 || group == 1 || isPowerOf(group, 3) ||
        isPowerOf(group, 5) || isPowerOf(group, 7)) {
      size_t sb_bid = sb.s_first_data_block_ + group * sb.s_blocks_per_group_;
      bm_->tagBlock(sb_bid, 1);
      bm_->tagBlock(sb_bid + 1, 1);
    } else {
      size_t bgd_bid = sb.s_first_data_block_ + group * sb.s_blocks_per_group_;
      bm_->tagBlock(bgd_bid, 1);
    }
    bm_->tagBlock(bm_->bgd_[group].bg_block_bitmap_, 1);
    bm_->tagBlock(bm_->bgd_[group].bg_inode_bitmap_, 1);
    for (int i = 0; i < 214; i++) {
      bm_->tagBlock(bm_->bgd_[group].bg_inode_table_ + i, 1);
    }
  }
  inode root_inode;
  memset(&root_inode, 0, sizeof(inode));
  root_inode.i_mode_ = EXT2_S_IFDIR | 777;
  root_inode.i_links_count_ = 2;

  //
  auto iid = im_->new_inode(root_inode);
  assert(iid == 1);

  im_->dir_add_dentry(1, 1, ".", EXT2_FT_DIR);
  im_->dir_add_dentry(1, 1, "..", EXT2_FT_DIR);
}