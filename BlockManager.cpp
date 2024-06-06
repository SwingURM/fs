#include "BlockManager.h"

#include <cassert>
#include <cstring>

#include "util.h"
#ifndef DEPLOY
#include <iostream>
#endif

SuperBlockManager::SuperBlockManager(std::shared_ptr<MyDisk> bd)
    : bd_(bd), sb_() {
  assert(BLOCK_SIZE == 1024);
  auto sb = bd_->bread(1);
  memcpy(&sb_, sb->s_, sizeof(SuperBlock));
}

const SuperBlock& SuperBlockManager::readSuperBlock() { return sb_; }

bool SuperBlockManager::writeSuperBlock(const SuperBlock* const sb) {
  assert(BLOCK_SIZE == 1024);
  memcpy(&sb_, sb, sizeof(SuperBlock));
  DeviceBlock db;
  memcpy(db.s_, sb, sizeof(SuperBlock));
  bd_->bwrite(&db, 1);
  return true;
}

BlockManager::BlockManager(std::shared_ptr<MyDisk> bd,
                           std::shared_ptr<SuperBlockManager> sbm)
    : bd_(bd), sbm_(sbm), bgd_() {}

void BlockManager::refresh() {
  auto sb = sbm_->readSuperBlock();
  auto bgd_block = readBlock(1 + sb.s_first_data_block_);
  auto total_groups = (sb.s_blocks_count_ - sb.s_first_data_block_ +
                       sb.s_blocks_per_group_ - 1) /
                      sb.s_blocks_per_group_;
  bgd_.clear();
  for (int i = 0; i < total_groups; i++) {
    bgd_.push_back(*reinterpret_cast<Block_Group_Descriptor*>(
        bgd_block.s_.get() + i * sizeof(Block_Group_Descriptor)));
  }
}

bool BlockManager::tagBlock(uint32_t index, bool val) {
  auto sb = sbm_->readSuperBlock();
  auto maxBlock = sb.s_blocks_count_;
  assert(index >= 0 && index < maxBlock &&
         "Index out of range");  // 确保索引在合法范围内
  auto group = (index - sb.s_first_data_block_) / sb.s_blocks_per_group_;
  auto offset = (index - sb.s_first_data_block_) % sb.s_blocks_per_group_;
  assert(group < bgd_.size() && "Group out of range");
  auto bitmap_bid = bgd_[group].bg_block_bitmap_;
  auto bitmap = readBlock(bitmap_bid);
  int byteIndex = offset / 8;
  int bitIndex = offset % 8;
  if (val) {
    bitmap.s_[byteIndex] |= val << bitIndex;
#ifndef DEPLOY
    std::cout << "Allocating block " << index << std::endl;
#endif
  } else {
    bitmap.s_[byteIndex] &= ~(1 << bitIndex);
#ifndef DEPLOY
    std::cout << "Deallocating block " << index << std::endl;
#endif
  }
  writeBlock(bitmap, bitmap_bid);
  return true;
}

bool BlockManager::state(uint32_t index) const {
  auto sb = sbm_->readSuperBlock();
  auto maxBlock = sb.s_blocks_count_;
  assert(index >= 0 && index < maxBlock &&
         "Index out of range");  // 确保索引在合法范围内
  auto group = (index - sb.s_first_data_block_) / sb.s_blocks_per_group_;
  auto offset = (index - sb.s_first_data_block_) % sb.s_blocks_per_group_;
  assert(group < bgd_.size() && "Group out of range");
  auto bitmap_bid = bgd_[group].bg_block_bitmap_;
  auto bitmap = readBlock(bitmap_bid);
  int byteIndex = offset / 8;
  int bitIndex = offset % 8;
  return bitmap.s_[byteIndex] & (1 << bitIndex);
}

uint32_t BlockManager::getIdleBlock() {
  auto sb = sbm_->readSuperBlock();
  auto inode_per_tbl = (1024 << sb.s_log_block_size_) / sizeof(inode);
  auto i_tbl_size =
      (sb.s_inodes_per_group_ + inode_per_tbl - 1) / inode_per_tbl;

  for (uint32_t group = 0; group < bgd_.size(); group++) {
    auto bitmap_bid = bgd_[group].bg_block_bitmap_;
    auto bitmap = readBlock(bitmap_bid);
    uint32_t remainBlock = sb.s_blocks_count_ - sb.s_first_data_block_ -
                           group * sb.s_blocks_per_group_;
    for (uint32_t i = 0; i < std::min(remainBlock, sb.s_blocks_per_group_);
         i++) {
      int byteIndex = i / 8;
      int bitIndex = i % 8;
      if (((bitmap.s_[byteIndex] >> bitIndex) & 1) == 0) {
        bitmap.s_[byteIndex] |= 1 << bitIndex;
        writeBlock(bitmap, bitmap_bid);
        // wipe
        auto abs_bid =
            sb.s_first_data_block_ + group * sb.s_blocks_per_group_ + i;
        // TODO move to where should deal with this
        // if (group == 0 || group == 1 || isPowerOf(group, 3) ||
        //     isPowerOf(group, 5) || isPowerOf(group, 7))
        //   // has superblock, group descriptor copy
        //   abs_bid += 2;
#ifndef DEPLOY
        std::cout << "Allocating block " << abs_bid << "(" << i << ")"
                  << std::endl;
#endif
        FSBlock assigned{
            std::make_unique<char[]>(1024 << sb.s_log_block_size_)};
        // memset(assigned.s_.get(), 0, 1024 << sb.s_log_block_size_);
        writeBlock(assigned, abs_bid);
        return abs_bid;
      }
    }
  }
  assert(0);
}

FSBlock BlockManager::readBlock(uint32_t bid) const {
  auto slbs = sbm_->readSuperBlock().s_log_block_size_;
  assert(BLOCK_SIZE <= 1024 << slbs);
  uint32_t start = bid << slbs;
  uint32_t end = (bid + 1) << slbs;
  FSBlock block{std::make_unique<char[]>(1024 << slbs)};
  for (uint32_t i = start; i < end; i++) {
    auto db = bd_->bread(i);
    memcpy(block.s_.get() + (i - start) * BLOCK_SIZE, db->s_, BLOCK_SIZE);
  }
  return block;
}
bool BlockManager::writeBlock(const FSBlock& block, uint32_t bid) {
  auto slbs = sbm_->readSuperBlock().s_log_block_size_;
  assert(BLOCK_SIZE <= 1024 << slbs);
  uint32_t start = bid << slbs;
  uint32_t end = (bid + 1) << slbs;
  for (uint32_t i = start; i < end; i++) {
    auto data = reinterpret_cast<DeviceBlock*>(block.s_.get() +
                                               (i - start) * BLOCK_SIZE);
    bd_->bwrite(data, i);
  }
  return true;
}