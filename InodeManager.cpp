#include "InodeManager.h"

#include <cassert>
#include <cstring>
#ifndef DEPLOY
#include <iostream>
#endif
InodeManager::InodeManager(std::shared_ptr<SuperBlockManager> sbm,
                           std::shared_ptr<BlockManager> bm)
    : sbm_(sbm), bm_(bm) {}

bool InodeManager::getIdleInode(uint32_t* iid) {
  auto sb = sbm_->readSuperBlock();
  auto inode_per_tbl = (1024 << sb.s_log_block_size_) / sizeof(inode);
  auto i_tbl_size =
      (sb.s_inodes_per_group_ + inode_per_tbl - 1) / inode_per_tbl;
  for (uint32_t group = 0; group < bm_->bgd_.size(); group++) {
    auto bitmap_bid = bm_->bgd_[group].bg_inode_bitmap_;
    auto bitmap = bm_->readBlock(bitmap_bid);
    for (uint32_t i = 0; i < sb.s_inodes_per_group_; i++) {
      int byteIndex = i / 8;
      int bitIndex = i % 8;
      if (((bitmap.s_[byteIndex] >> bitIndex) & 1) == 0) {
        bitmap.s_[byteIndex] |= 1 << bitIndex;
        bm_->writeBlock(bitmap, bitmap_bid);
        *iid = group * sb.s_inodes_per_group_ + i + 1;
        return true;
      }
    }
  }
  return false;
}

uint32_t InodeManager::new_inode(const inode& in) {
  uint32_t iid;
  assert(getIdleInode(&iid));

  auto sb = sbm_->readSuperBlock();
  int block_group = (iid - 1) / sb.s_inodes_per_group_;
  int local_inode_index = (iid - 1) % sb.s_inodes_per_group_;

  // edit inode table
  auto inode_per_blk = (1024 << sb.s_log_block_size_) / sizeof(inode);
  auto inode_tbl_bid = bm_->bgd_[block_group].bg_inode_table_ +
                       local_inode_index / inode_per_blk;
  auto local_inode_index_in_block = local_inode_index % inode_per_blk;
  auto inode_tbl_block = bm_->readBlock(inode_tbl_bid);
  auto inode_tbl = reinterpret_cast<inode_table*>(inode_tbl_block.s_.get());
  inode_tbl->inodes_[local_inode_index_in_block] = in;
  bm_->writeBlock(inode_tbl_block, inode_tbl_bid);
  return iid;
}

bool InodeManager::del_inode(uint32_t iid) {
  auto sb = sbm_->readSuperBlock();
  int block_group = (iid - 1) / sb.s_inodes_per_group_;
  int local_inode_index = (iid - 1) % sb.s_inodes_per_group_;
  auto bitmap_bid = bm_->bgd_[block_group].bg_inode_bitmap_;
  auto inode_map_block = bm_->readBlock(bitmap_bid);
  int byteIndex = local_inode_index / 8;
  int bitIndex = local_inode_index % 8;
  assert(inode_map_block.s_[byteIndex] & (1 << bitIndex));
  inode_map_block.s_[byteIndex] &= ~(1 << bitIndex);
  bm_->writeBlock(inode_map_block, bitmap_bid);
  return true;
}

inode InodeManager::read_inode(uint32_t iid) const {
  auto sb = sbm_->readSuperBlock();
  int block_group = (iid - 1) / sb.s_inodes_per_group_;
  int local_inode_index = (iid - 1) % sb.s_inodes_per_group_;

  auto inode_per_blk = (1024 << sb.s_log_block_size_) / sizeof(inode);
  auto i_tbl_bid = bm_->bgd_[block_group].bg_inode_table_ +
                   local_inode_index / inode_per_blk;
  auto local_inode_index_in_block = local_inode_index % inode_per_blk;

  auto i_tbl = bm_->readBlock(i_tbl_bid);
  auto itbl = reinterpret_cast<inode_table*>(i_tbl.s_.get());
  return itbl->inodes_[local_inode_index_in_block];
}

bool InodeManager::write_inode(const inode& in, uint32_t iid) {
  auto sb = sbm_->readSuperBlock();
  int block_group = (iid - 1) / sb.s_inodes_per_group_;
  int local_inode_index = (iid - 1) % sb.s_inodes_per_group_;

  auto inode_per_blk = (1024 << sb.s_log_block_size_) / sizeof(inode);
  auto i_tbl_bid = bm_->bgd_[block_group].bg_inode_table_ +
                   local_inode_index / inode_per_blk;
  auto local_inode_index_in_block = local_inode_index % inode_per_blk;

  auto i_tbl = bm_->readBlock(i_tbl_bid);
  auto itbl = reinterpret_cast<inode_table*>(i_tbl.s_.get());
  itbl->inodes_[local_inode_index_in_block] = in;
  bm_->writeBlock(i_tbl, i_tbl_bid);
  return true;
}
size_t InodeManager::write_inode_data(uint32_t iid, const void* src,
                                      size_t offset, size_t size) const {
  auto block_size = 1024 << sbm_->readSuperBlock().s_log_block_size_;
  auto in = read_inode(iid);
  size_t written = 0;
  while (written < size) {
    size_t cur = offset + written;
    size_t next_boundary = (cur + block_size) / block_size * block_size;
    assert(next_boundary >= offset);
    written +=
        write_inode_data_helper(in, (char*)src + written, cur,
                                std::min(size - written, next_boundary - cur));
  }
  assert(written == size);
  return written;
}

size_t InodeManager::read_inode_data(uint32_t iid, void* dst, size_t offset,
                                     size_t size) const {
  auto block_size = 1024 << sbm_->readSuperBlock().s_log_block_size_;
  auto in = read_inode(iid);
  size_t readed = 0;
  while (readed < size) {
    size_t cur = offset + readed;
    size_t next_boundary = (cur + block_size) / block_size * block_size;
    assert(next_boundary >= offset);
    readed +=
        read_inode_data_helper(in, (char*)dst + readed, cur,
                               std::min(size - readed, next_boundary - cur));
  }
  assert(readed == size);
  return readed;
}

size_t InodeManager::read_inode_data_helper(const inode& in, void* dst,
                                            size_t offset, size_t size) const {
  auto slbs = sbm_->readSuperBlock().s_log_block_size_;
  auto block_size = 1024 << slbs;
  auto n_entries = block_size / sizeof(uint32_t);
  assert(size + (offset % block_size) <= block_size);
  int used_block = in.i_blocks_ / (2 << slbs);
  size_t bid = offset / block_size;
  size_t local_offset = offset % block_size;
  if (bid < NDIRECT_BLOCK) {
    assert(in.i_block_[bid]);
    assert(bm_->state(in.i_block_[bid]));
    auto block = bm_->readBlock(in.i_block_[bid]);
    size_t read_size = std::min(size, block_size - local_offset);
    memcpy(dst, block.s_.get() + local_offset, read_size);
    return read_size;
  } else if (bid < NDIRECT_BLOCK + N1INDIRECT_BLOCK * n_entries) {
    assert(in.i_block_[NDIRECT_BLOCK]);
    assert(bm_->state(in.i_block_[NDIRECT_BLOCK]));
    auto b1 = bm_->readBlock(in.i_block_[NDIRECT_BLOCK]);
    auto local_bid = bid - NDIRECT_BLOCK;
    auto bid1 = local_bid;
    assert(*((uint32_t*)b1.s_.get() + bid1));
    assert(bm_->state(*((uint32_t*)b1.s_.get() + bid1)));
    auto block = bm_->readBlock(*((uint32_t*)b1.s_.get() + bid1));
    size_t read_size = std::min(size, block_size - local_offset);
    memcpy(dst, block.s_.get() + local_offset, read_size);
    return read_size;
  } else if (bid < NDIRECT_BLOCK + N1INDIRECT_BLOCK * n_entries +
                       N2INDIRECT_BLOCK * n_entries * n_entries) {
    assert(in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK]);
    assert(bm_->state(in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK]));
    auto b1 = bm_->readBlock(in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK]);
    auto local_bid = bid - NDIRECT_BLOCK - N1INDIRECT_BLOCK * n_entries;
    auto bid1 = local_bid / n_entries;
    auto bid2 = local_bid % n_entries;
    assert(*((uint32_t*)b1.s_.get() + bid1));
    assert(bm_->state(*((uint32_t*)b1.s_.get() + bid1)));
    auto b2 = bm_->readBlock(*((uint32_t*)b1.s_.get() + bid1));
    assert(*((uint32_t*)b2.s_.get() + bid2));
    assert(bm_->state(*((uint32_t*)b2.s_.get() + bid2)));
    auto block = bm_->readBlock(*((uint32_t*)b2.s_.get() + bid2));

    size_t read_size = std::min(size, block_size - local_offset);
    memcpy(dst, block.s_.get() + local_offset, read_size);
    return read_size;
  } else if (bid < NDIRECT_BLOCK + N1INDIRECT_BLOCK * n_entries +
                       N2INDIRECT_BLOCK * n_entries * n_entries +
                       N3INDIRECT_BLOCK * n_entries * n_entries * n_entries) {
    assert(in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK]);
    assert(bm_->state(
        in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK]));
    auto b1 = bm_->readBlock(
        in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK]);
    auto local_bid = bid - NDIRECT_BLOCK - N1INDIRECT_BLOCK * n_entries -
                     N2INDIRECT_BLOCK * n_entries * n_entries;
    auto bid1 = local_bid / n_entries / n_entries;
    auto bid2 = local_bid / n_entries % n_entries;
    auto bid3 = local_bid % n_entries;
    assert(*((uint32_t*)b1.s_.get() + bid1));
    assert(bm_->state(*((uint32_t*)b1.s_.get() + bid1)));
    auto b2 = bm_->readBlock(*((uint32_t*)b1.s_.get() + bid1));
    assert(*((uint32_t*)b2.s_.get() + bid2));
    assert(bm_->state(*((uint32_t*)b2.s_.get() + bid2)));
    auto b3 = bm_->readBlock(*((uint32_t*)b2.s_.get() + bid2));
    assert(*((uint32_t*)b3.s_.get() + bid3));
    assert(bm_->state(*((uint32_t*)b3.s_.get() + bid3)));
    auto block = bm_->readBlock(*((uint32_t*)b3.s_.get() + bid3));
    size_t read_size = std::min(size, block_size - local_offset);
    memcpy(dst, block.s_.get() + local_offset, read_size);
    return read_size;
  } else {
    assert(0);
  }
}
size_t InodeManager::write_inode_data_helper(const inode& in, void* src,
                                             size_t offset, size_t size) const {
  auto slbs = sbm_->readSuperBlock().s_log_block_size_;
  auto block_size = 1024 << slbs;
  auto n_entries = block_size / sizeof(uint32_t);
  assert(size + (offset % block_size) <= block_size);
  int used_block = in.i_blocks_ / (2 << slbs);
  size_t bid = offset / block_size;
  size_t local_offset = offset % block_size;

  if (bid < NDIRECT_BLOCK) {
    assert(in.i_block_[bid]);
    assert(bm_->state(in.i_block_[bid]));
    auto block = bm_->readBlock(in.i_block_[bid]);
    size_t write_size = std::min(size, block_size - local_offset);
    memcpy(block.s_.get() + local_offset, src, write_size);
    bm_->writeBlock(block, in.i_block_[bid]);
    return write_size;
  } else if (bid < NDIRECT_BLOCK + N1INDIRECT_BLOCK * n_entries) {
    assert(in.i_block_[NDIRECT_BLOCK]);
    assert(bm_->state(in.i_block_[NDIRECT_BLOCK]));
    auto b1 = bm_->readBlock(in.i_block_[NDIRECT_BLOCK]);
    auto local_bid = bid - NDIRECT_BLOCK;
    auto bid1 = local_bid;
    assert(*((uint32_t*)b1.s_.get() + bid1));
    assert(bm_->state(*((uint32_t*)b1.s_.get() + bid1)));
    auto block = bm_->readBlock(*((uint32_t*)b1.s_.get() + bid1));
    size_t write_size = std::min(size, block_size - local_offset);
    memcpy(block.s_.get() + local_offset, src, write_size);
    bm_->writeBlock(block, *((uint32_t*)b1.s_.get() + bid1));
    return write_size;
  } else if (bid < NDIRECT_BLOCK + N1INDIRECT_BLOCK * n_entries +
                       N2INDIRECT_BLOCK * n_entries * n_entries) {
    assert(in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK]);
    assert(bm_->state(in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK]));
    auto b1 = bm_->readBlock(in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK]);
    auto local_bid = bid - NDIRECT_BLOCK - N1INDIRECT_BLOCK * n_entries;
    auto bid1 = local_bid / n_entries;
    auto bid2 = local_bid % n_entries;
    assert(*((uint32_t*)b1.s_.get() + bid1));
    assert(bm_->state(*((uint32_t*)b1.s_.get() + bid1)));
    auto b2 = bm_->readBlock(*((uint32_t*)b1.s_.get() + bid1));
    assert(*((uint32_t*)b2.s_.get() + bid2));
    assert(bm_->state(*((uint32_t*)b2.s_.get() + bid2)));
    auto block = bm_->readBlock(*((uint32_t*)b2.s_.get() + bid2));

    size_t write_size = std::min(size, block_size - local_offset);
    memcpy(block.s_.get() + local_offset, src, write_size);
    bm_->writeBlock(block, *((uint32_t*)b2.s_.get() + bid2));
    return write_size;
  } else if (bid < NDIRECT_BLOCK + N1INDIRECT_BLOCK * n_entries +
                       N2INDIRECT_BLOCK * n_entries * n_entries +
                       N3INDIRECT_BLOCK * n_entries * n_entries * n_entries) {
    assert(in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK]);
    assert(bm_->state(
        in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK]));
    auto b1 = bm_->readBlock(
        in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK]);
    auto local_bid = bid - NDIRECT_BLOCK - N1INDIRECT_BLOCK * n_entries -
                     N2INDIRECT_BLOCK * n_entries * n_entries;
    auto bid1 = local_bid / n_entries / n_entries;
    auto bid2 = local_bid / n_entries % n_entries;
    auto bid3 = local_bid % n_entries;
    assert(*((uint32_t*)b1.s_.get() + bid1));
    assert(bm_->state(*((uint32_t*)b1.s_.get() + bid1)));
    auto b2 = bm_->readBlock(*((uint32_t*)b1.s_.get() + bid1));
    assert(*((uint32_t*)b2.s_.get() + bid2));
    assert(bm_->state(*((uint32_t*)b2.s_.get() + bid2)));
    auto b3 = bm_->readBlock(*((uint32_t*)b2.s_.get() + bid2));
    assert(*((uint32_t*)b3.s_.get() + bid3));
    assert(bm_->state(*((uint32_t*)b3.s_.get() + bid3)));
    auto block = bm_->readBlock(*((uint32_t*)b3.s_.get() + bid3));
    size_t write_size = std::min(size, block_size - local_offset);
    memcpy(block.s_.get() + local_offset, src, write_size);
    bm_->writeBlock(block, *((uint32_t*)b3.s_.get() + bid3));
    return write_size;
  } else {
    assert(0);
  }
}

bool InodeManager::find_next(inode in, const std::string& dir, uint32_t* ret) {
  for (auto it = dentry_begin(&in); it != dentry_end(&in); ++it) {
    if (it.cur_dentry_name() == dir) {
      *ret = it.cur_dentry().inode_;
      return true;
    }
  }
  return false;
}
bool InodeManager::dir_add_dentry(uint32_t dst, uint32_t src,
                                  const std::string& name) {
  auto in = read_inode(dst);
  assert(in.i_mode_ & EXT2_S_IFDIR);
  auto block_size = 1024 << sbm_->readSuperBlock().s_log_block_size_;
  // Construct dentry
  assert(name.size() < 256);
  dentry d = {src, static_cast<uint16_t>(UPPER4(sizeof(dentry) + name.size())),
              static_cast<uint8_t>(name.size()), EXT2_FT_REG_FILE};
  if (in.i_size_ % block_size + d.rec_len_ > block_size) {
    // modify the rec len of the last dentry
    auto prev = dentry_begin(&in);
    for (auto it = dentry_begin(&in); it != dentry_end(&in); ++it) {
      prev = it;
    }
    auto prev_d = prev.cur_dentry();
    auto new_rec_len = block_size - prev.offset_ % block_size;
    auto new_i_size = in.i_size_ + new_rec_len - prev_d.rec_len_;
    prev_d.rec_len_ = new_rec_len;
    write_inode_data(dst, &prev_d, prev.offset_, sizeof(dentry));
    resize(dst, new_i_size);
    in = read_inode(dst);
  }
  auto write_offset = in.i_size_;
  resize(dst, in.i_size_ + d.rec_len_);
  write_inode_data(dst, &d, write_offset, sizeof(dentry));
  write_inode_data(dst, name.c_str(), write_offset + sizeof(dentry),
                   name.size());
  return true;
}
bool InodeManager::dir_del_dentry(uint32_t dst, const std::string& name) {
  auto in = read_inode(dst);
  assert(in.i_mode_ & EXT2_S_IFDIR);
  auto prev = dentry_begin(&in);
  for (auto it = dentry_begin(&in); it != dentry_end(&in); ++it) {
    if (it.cur_dentry_name() == name) {
      assert(it != dentry_begin(&in));
      auto prev_dentry = prev.cur_dentry();
      auto cur_dentry = it.cur_dentry();
      prev_dentry.rec_len_ += cur_dentry.rec_len_;
      write_inode_data(dst, &prev_dentry, prev.offset_, sizeof(dentry));
      return true;
    }
    prev = it;
  }
  assert(0);
}

bool InodeManager::dir_empty(uint32_t dst) {
  auto in = read_inode(dst);
  assert(in.i_mode_ & EXT2_S_IFDIR);
  if (in.i_links_count_ == 2) {
    // only . and ..
    return true;
  }
  for (auto it = dentry_begin(&in); it != dentry_end(&in); ++it) {
    if (it.cur_dentry_name() != "." && it.cur_dentry_name() != "..") {
      return false;
    }
  }
  return true;
}

void InodeManager::resize(int iid, uint32_t size) {
  auto inode = read_inode(iid);
  auto slbs = sbm_->readSuperBlock().s_log_block_size_;
  auto block_size = 1024 << slbs;
  auto n_entries = block_size / sizeof(uint32_t);
  int used_block = inode.i_blocks_ / (2 << slbs);

  int after_block = (size + block_size - 1) / block_size;

  if (used_block == after_block) {
    inode.i_size_ = size;
    write_inode(inode, iid);
    return;
  } else if (used_block > after_block) {
    // Free direct blocks
    for (int i = after_block; i < used_block && i < NDIRECT_BLOCK; ++i) {
      assert(inode.i_block_[i]);
      bm_->tagBlock(inode.i_block_[i], 0);
      inode.i_block_[i] = 0;
    }

    // Free indirect blocks
    if (used_block > NDIRECT_BLOCK) {
      unsigned int start =
          (after_block > NDIRECT_BLOCK) ? after_block - NDIRECT_BLOCK : 0;
      unsigned int end =
          used_block - NDIRECT_BLOCK > N1INDIRECT_BLOCK * n_entries
              ? N1INDIRECT_BLOCK * n_entries
              : used_block - NDIRECT_BLOCK;
      if (free_indirect_blocks(inode.i_block_[NDIRECT_BLOCK], 1, start, end))
        inode.i_block_[NDIRECT_BLOCK] = 0;
    }

    // Free double indirect blocks
    if (used_block > NDIRECT_BLOCK + N1INDIRECT_BLOCK * n_entries) {
      unsigned int start =
          after_block > NDIRECT_BLOCK + N1INDIRECT_BLOCK * n_entries
              ? after_block - (NDIRECT_BLOCK + N1INDIRECT_BLOCK * n_entries)
              : 0;
      unsigned int end =
          used_block - (NDIRECT_BLOCK + N1INDIRECT_BLOCK * n_entries) >
                  N2INDIRECT_BLOCK * n_entries * n_entries
              ? N2INDIRECT_BLOCK * n_entries * n_entries
              : used_block - (NDIRECT_BLOCK + N1INDIRECT_BLOCK * n_entries);
      if (free_indirect_blocks(inode.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK],
                               2, start, end))
        inode.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK] = 0;
    }

    // Free triple indirect blocks
    if (used_block > NDIRECT_BLOCK + N1INDIRECT_BLOCK * n_entries +
                         N2INDIRECT_BLOCK * n_entries * n_entries) {
      unsigned int start =
          after_block > NDIRECT_BLOCK + N1INDIRECT_BLOCK * n_entries +
                            N2INDIRECT_BLOCK * n_entries * n_entries
              ? after_block - (NDIRECT_BLOCK + N1INDIRECT_BLOCK * n_entries +
                               N2INDIRECT_BLOCK * n_entries * n_entries)
              : 0;
      unsigned int end =
          used_block - (NDIRECT_BLOCK + N1INDIRECT_BLOCK * n_entries +
                        N2INDIRECT_BLOCK * n_entries * n_entries) >
                  N3INDIRECT_BLOCK * n_entries * n_entries * n_entries
              ? N3INDIRECT_BLOCK * n_entries * n_entries * n_entries
              : used_block - (NDIRECT_BLOCK + N1INDIRECT_BLOCK * n_entries +
                              N2INDIRECT_BLOCK * n_entries * n_entries);
      if (free_indirect_blocks(inode.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK +
                                              N2INDIRECT_BLOCK],
                               3, start, end))
        inode.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK] = 0;
    }
  } else {
    for (int i = used_block; i < after_block; i++) {
      allocate_data(inode, i);
    }
  }
  inode.i_size_ = size;
  inode.i_blocks_ = (size + block_size - 1) / block_size * (2 << slbs);
  write_inode(inode, iid);
}

bool InodeManager::free_indirect_blocks(uint32_t bid, int level, size_t start,
                                        size_t end) {
  auto block = bm_->readBlock(bid);
  auto n_entries =
      (1024 << sbm_->readSuperBlock().s_log_block_size_) / sizeof(uint32_t);
  if (level == 1) {
    for (size_t i = start; i < end; i++) {
      auto entry = *((uint32_t*)block.s_.get() + i);
      assert(entry);
      bm_->tagBlock(entry, 0);
      *((uint32_t*)block.s_.get() + i) = 0;
    }
    if (start == 0) {
      bm_->tagBlock(bid, 0);
      return true;
    }
    return false;
  }

  int level_entries = 1;
  for (int i = 1; i < level; ++i) {
    level_entries *= n_entries;
  }

  size_t start_index = start / level_entries;
  size_t end_index = end / level_entries;
  unsigned int sub_start, sub_end;

  for (size_t i = start_index; i <= end_index; ++i) {
    auto entry = *((uint32_t*)block.s_.get() + i);
    assert(entry);
    sub_start = (i == start_index) ? start % level_entries : 0;
    sub_end = (i == end_index) ? end % level_entries : level_entries;
    if (free_indirect_blocks(entry, level - 1, sub_start, sub_end))
      *((uint32_t*)block.s_.get() + i) = 0;
  }
  bm_->writeBlock(block, bid);
  if (start_index == 0) {
    bm_->tagBlock(bid, 0);
    return true;
  }
  return false;
}

void InodeManager::allocate_data(inode& in, size_t bid) {
  auto block_size = 1024 << sbm_->readSuperBlock().s_log_block_size_;
  auto n_entries = block_size / sizeof(uint32_t);
  if (bid < NDIRECT_BLOCK) {
    in.i_block_[bid] = bm_->getIdleBlock();
    return;
  } else if (bid < NDIRECT_BLOCK + N1INDIRECT_BLOCK * n_entries) {
    FSBlock b1{nullptr};
    if (in.i_block_[NDIRECT_BLOCK] == 0) {
      in.i_block_[NDIRECT_BLOCK] = bm_->getIdleBlock();
      b1 = bm_->readBlock(in.i_block_[NDIRECT_BLOCK]);
    } else {
      b1 = bm_->readBlock(in.i_block_[NDIRECT_BLOCK]);
    }
    auto local_bid = bid - NDIRECT_BLOCK;
    auto bid1 = local_bid;
    *((uint32_t*)b1.s_.get() + bid1) = bm_->getIdleBlock();
    bm_->writeBlock(b1, in.i_block_[NDIRECT_BLOCK]);
    return;
  } else if (bid < NDIRECT_BLOCK + N1INDIRECT_BLOCK * n_entries +
                       N2INDIRECT_BLOCK * n_entries * n_entries) {
    FSBlock b1{nullptr};
    bool modified = false;
    if (in.i_block_[N1INDIRECT_BLOCK + NDIRECT_BLOCK] == 0) {
      in.i_block_[N1INDIRECT_BLOCK + NDIRECT_BLOCK] = bm_->getIdleBlock();
      b1 = bm_->readBlock(in.i_block_[N1INDIRECT_BLOCK + NDIRECT_BLOCK]);
      modified |= true;
    } else {
      b1 = bm_->readBlock(in.i_block_[N1INDIRECT_BLOCK + NDIRECT_BLOCK]);
    }
    auto local_bid = bid - NDIRECT_BLOCK - N1INDIRECT_BLOCK * n_entries;
    auto bid1 = local_bid / n_entries;
    auto bid2 = local_bid % n_entries;
    FSBlock b2;
    if (*((uint32_t*)b1.s_.get() + bid1) == 0) {
      *((uint32_t*)b1.s_.get() + bid1) = bm_->getIdleBlock();
      b2 = bm_->readBlock(*((uint32_t*)b1.s_.get() + bid1));
      modified |= true;
    } else {
      b2 = bm_->readBlock(*((uint32_t*)b1.s_.get() + bid1));
    }
    if (modified)
      bm_->writeBlock(b1, in.i_block_[N1INDIRECT_BLOCK + NDIRECT_BLOCK]);
    *((uint32_t*)b2.s_.get() + bid2) = bm_->getIdleBlock();
    bm_->writeBlock(b2, *((uint32_t*)b1.s_.get() + bid1));
    return;
  } else if (bid < NDIRECT_BLOCK + N1INDIRECT_BLOCK * n_entries +
                       N2INDIRECT_BLOCK * n_entries * n_entries +
                       N3INDIRECT_BLOCK * n_entries * n_entries * n_entries) {
    FSBlock b1;
    bool modified = false;
    if (in.i_block_[N1INDIRECT_BLOCK + N2INDIRECT_BLOCK + NDIRECT_BLOCK] == 0) {
      in.i_block_[N1INDIRECT_BLOCK + N2INDIRECT_BLOCK + NDIRECT_BLOCK] =
          bm_->getIdleBlock();
      b1 = bm_->readBlock(
          in.i_block_[N1INDIRECT_BLOCK + N2INDIRECT_BLOCK + NDIRECT_BLOCK]);
      modified |= true;
    } else {
      b1 = bm_->readBlock(
          in.i_block_[N1INDIRECT_BLOCK + N2INDIRECT_BLOCK + NDIRECT_BLOCK]);
    }
    auto local_bid = bid - NDIRECT_BLOCK - N1INDIRECT_BLOCK * n_entries -
                     N2INDIRECT_BLOCK * n_entries * n_entries;
    auto bid1 = local_bid / n_entries / n_entries;
    auto bid2 = local_bid / n_entries % n_entries;
    auto bid3 = local_bid % n_entries;
    FSBlock b2;
    if (*((uint32_t*)b1.s_.get() + bid1) == 0) {
      *((uint32_t*)b1.s_.get() + bid1) = bm_->getIdleBlock();
      b2 = bm_->readBlock(*((uint32_t*)b1.s_.get() + bid1));
      modified |= true;
    } else {
      b2 = bm_->readBlock(*((uint32_t*)b1.s_.get() + bid1));
    }
    if (modified) {
      bm_->writeBlock(
          b1, in.i_block_[N1INDIRECT_BLOCK + N2INDIRECT_BLOCK + NDIRECT_BLOCK]);
      modified = false;
    }
    FSBlock b3;
    if (*((uint32_t*)b2.s_.get() + bid2) == 0) {
      *((uint32_t*)b2.s_.get() + bid2) = bm_->getIdleBlock();
      b3 = bm_->readBlock(*((uint32_t*)b2.s_.get() + bid2));
      modified |= true;
    } else {
      b3 = bm_->readBlock(*((uint32_t*)b2.s_.get() + bid2));
    }
    if (modified) bm_->writeBlock(b2, *((uint32_t*)b1.s_.get() + bid1));
    *((uint32_t*)b3.s_.get() + bid3) = bm_->getIdleBlock();
    bm_->writeBlock(b3, *((uint32_t*)b2.s_.get() + bid2));
    return;
  }
}

InodeManager::dentry_iterator::dentry_iterator(inode* in, InodeManager* im,
                                               size_t offset)
    : dinode_(in), im_(im), offset_(offset) {}

dentry InodeManager::dentry_iterator::cur_dentry() const {
  assert(offset_ != dinode_->i_size_);
  dentry ret;
  auto read_size =
      im_->read_inode_data_helper(*dinode_, &ret, offset_, sizeof(dentry));
  assert(read_size == sizeof(dentry));
  return ret;
}
std::string InodeManager::dentry_iterator::cur_dentry_name() const {
  assert(offset_ != dinode_->i_size_);
  auto dentry = cur_dentry();
  char name[256];
  auto read_size = im_->read_inode_data_helper(
      *dinode_, name, offset_ + sizeof(dentry), dentry.name_len_);
  assert(read_size == dentry.name_len_);
  return std::string(name, dentry.name_len_);
}
InodeManager::dentry_iterator& InodeManager::dentry_iterator::operator++() {
  assert(offset_ != dinode_->i_size_);
  auto dentry = cur_dentry();
  offset_ += dentry.rec_len_;
  return *this;
}

InodeManager::dentry_iterator InodeManager::dentry_begin(inode* in) {
  return InodeManager::dentry_iterator(in, this, 0);
}
InodeManager::dentry_iterator InodeManager::dentry_end(inode* in) {
  return InodeManager::dentry_iterator(in, this, in->i_size_);
}

bool operator==(const InodeManager::dentry_iterator& lhs,
                const InodeManager::dentry_iterator& rhs) {
  if (memcmp(lhs.dinode_, rhs.dinode_, sizeof(inode)) != 0) {
    return false;
  }
  return lhs.offset_ == rhs.offset_;
}

bool operator!=(const InodeManager::dentry_iterator& lhs,
                const InodeManager::dentry_iterator& rhs) {
  if (memcmp(lhs.dinode_, rhs.dinode_, sizeof(inode)) != 0) {
    return true;
  }
  return lhs.offset_ != rhs.offset_;
}
