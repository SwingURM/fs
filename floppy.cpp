#include "floppy.h"

#include <cassert>
#include <cstring>
#include <ctime>
#include "util.h"
#ifndef DEPLOY
#include <iostream>
#endif
SuperBlockManager::SuperBlockManager(std::shared_ptr<MyDisk> bd) : bd_(bd) {}

std::unique_ptr<SuperBlock> SuperBlockManager::readSuperBlock() {
  assert(BLOCK_SIZE == 1024);
  auto sb = bd_->bread(1);
  auto ret = std::make_unique<SuperBlock>();
  memcpy(ret.get(), sb->s_, sizeof(SuperBlock));
  return ret;
}

bool SuperBlockManager::writeSuperBlock(const SuperBlock* const sb) {
  assert(BLOCK_SIZE == 1024);
  block bl;
  memcpy(bl.s_, sb, sizeof(SuperBlock));
  bl.blockNo_ = 1;
  bd_->bwrite(&bl);
  return true;
}

FloppyDisk::FloppyDisk(std::shared_ptr<MyDisk> bd) {
  assert(bd->initialize());
  bd_ = bd;
  sbm_ = std::make_shared<SuperBlockManager>(bd);
  bm_ = std::make_shared<BlockManager>(bd, sbm_);
  im_ = std::make_shared<InodeManager>(bd, sbm_, bm_);
}
FloppyDisk::FloppyDisk(const std::string& filename) {
  bd_ = std::make_shared<MyDisk>(filename);
  assert(bd_->initialize());
  sbm_ = std::make_shared<SuperBlockManager>(bd_);
  bm_ = std::make_shared<BlockManager>(bd_, sbm_);
  im_ = std::make_shared<InodeManager>(bd_, sbm_, bm_);
}

void FloppyDisk::initialize() {
  auto sb = sbm_->readSuperBlock();
  // Initialize superblock
  // sb_.s_inodes_count_ = ;
  sb->s_blocks_count_ = 1439;
  // sb_.s_r_blocks_count_ = ;
  // sb_.s_free_blocks_count_ = 1439;
  // sb_.s_free_inodes_count_ = ;
  sb->s_first_data_block_ = 1;
  sb->s_log_block_size_ = 0;
  // sb_.s_log_frag_size_ = ;
  sb->s_blocks_per_group_ = 1439;
  // sb_.s_frags_per_group_ = ;
  sb->s_inodes_per_group_ =
      sb->s_blocks_per_group_ * (BLOCK_SIZE / sizeof(inode));
  sb->s_mtime_ = time(nullptr);
  sb->s_wtime_ = time(nullptr);
  // sb_.s_mnt_count_ = ;
  // sb_.s_max_mnt_count_ = ;
  sb->s_magic_ = EXT2_SUPER_MAGIC;
  // TODO 假设这里是挂载
  sb->s_state_ = EXT2_ERROR_FS;
  sbm_->writeSuperBlock(sb.get());

  // Initialize block group descriptor table

  // Initialize block bitmap
  auto block_bitmap_block = bd_->bread(3);
  std::memset(block_bitmap_block->s_, 0, BLOCK_SIZE);
  bd_->bwrite(block_bitmap_block.get());
  for (int i = 0; i < 28; i++) {
    // TODO: OPTIMIZE
    bm_->tagBlock(i, 1);
  }

  // Initialize inode bitmap
  // Initialize inode table
  auto inode_map_block = bd_->bread(4);
  memset(inode_map_block->s_, 0, BLOCK_SIZE);
  bd_->bwrite(inode_map_block.get());

  inode root_inode;
  memset(&root_inode, 0, sizeof(inode));
  root_inode.i_mode_ = EXT2_S_IFDIR;

  //
  auto iid = im_->new_inode(root_inode);
  assert(iid == 1);

  im_->dir_add_dentry(1, 1, ".");
  im_->dir_add_dentry(1, 1, "..");
}

// InodeManager
InodeManager::InodeManager(std::shared_ptr<MyDisk> bd,
                           std::shared_ptr<SuperBlockManager> sbm,
                           std::shared_ptr<BlockManager> bm)
    : bd_(bd), sbm_(sbm), bm_(bm) {}

uint32_t InodeManager::new_inode(const inode& in) {
  // TODO: ASSERT inode bitmap
  auto inode_map_block = bd_->bread(4);
  // find the first idle inode
  uint32_t local_iid;
  for (int i = 0; i < BLOCK_SIZE * 8; i++) {
    int byteIndex = i / 8;
    int bitIndex = i % 8;
    if (((inode_map_block->s_[byteIndex] >> bitIndex) & 1) == 0) {
      local_iid = i;
      inode_map_block->s_[byteIndex] |= 1 << bitIndex;
      bd_->bwrite(inode_map_block.get());
      break;
    }
  }
  uint32_t iid = local_iid + 1;

  auto sb = sbm_->readSuperBlock();
  int block_group = (iid - 1) / sb->s_inodes_per_group_;
  assert(block_group == 0);
  int local_inode_index = (iid - 1) % sb->s_inodes_per_group_;
  assert(local_inode_index == local_iid);

  // edit inode table
  auto inode_tbl_bid = local_inode_index / (BLOCK_SIZE / sizeof(inode)) + 5;
  auto local_inode_index_in_block =
      local_inode_index % (BLOCK_SIZE / sizeof(inode));
  auto inode_tbl_block = bd_->bread(inode_tbl_bid);
  auto inode_tbl = reinterpret_cast<inode_table*>(inode_tbl_block->s_);
  inode_tbl->inodes_[local_inode_index_in_block] = in;
  bd_->bwrite(inode_tbl_block.get());
  return iid;
}

bool InodeManager::del_inode(uint32_t iid) {
  // TODO: ASSERT inode bitmap
  auto inode_map_block = bd_->bread(4);
  uint32_t local_iid = iid - 1;
  int byteIndex = local_iid / 8;
  int bitIndex = local_iid % 8;
  assert((inode_map_block->s_[byteIndex] >> bitIndex) & 1);
  inode_map_block->s_[byteIndex] &= ~(1 << bitIndex);
  bd_->bwrite(inode_map_block.get());
  return true;
}

inode InodeManager::read_inode(uint32_t iid) const {
  auto sb = sbm_->readSuperBlock();
  int block_group = (iid - 1) / sb->s_inodes_per_group_;
  assert(block_group == 0);
  int local_inode_index = (iid - 1) % sb->s_inodes_per_group_;
  assert(local_inode_index < sb->s_inodes_per_group_);

  int block_group_bid = block_group * sb->s_blocks_per_group_ + 1;
  // TODO locating by bg_inode_table
  int inode_table_bid =
      block_group_bid + 4 + local_inode_index / (BLOCK_SIZE / sizeof(inode));
  int local_inode_index_in_block =
      local_inode_index % (BLOCK_SIZE / sizeof(inode));
  auto bp = bd_->bread(inode_table_bid);
  auto tp = reinterpret_cast<inode_table*>(bp->s_);
  return tp->inodes_[local_inode_index_in_block];
}

bool InodeManager::write_inode(const inode& in, uint32_t iid) {
  auto sb = sbm_->readSuperBlock();
  int block_group = (iid - 1) / sb->s_inodes_per_group_;
  assert(block_group == 0);
  int local_inode_index = (iid - 1) % sb->s_inodes_per_group_;
  assert(local_inode_index < sb->s_inodes_per_group_);

  int block_group_bid = block_group * sb->s_blocks_per_group_ + 1;
  // TODO locating by bg_inode_table
  int inode_table_bid =
      block_group_bid + 4 + local_inode_index / (BLOCK_SIZE / sizeof(inode));
  int local_inode_index_in_block =
      local_inode_index % (BLOCK_SIZE / sizeof(inode));
  auto bp = bd_->bread(inode_table_bid);
  auto tp = reinterpret_cast<inode_table*>(bp->s_);
  tp->inodes_[local_inode_index_in_block] = in;
  bd_->bwrite(bp.get());
  return true;
}
size_t InodeManager::write_inode_data(uint32_t iid, const void* src,
                                      size_t offset, size_t size) const {
  auto in = read_inode(iid);
  size_t written = 0;
  while (written < size) {
    size_t cur = offset + written;
    size_t next_boundary = (cur + BLOCK_SIZE) / BLOCK_SIZE * BLOCK_SIZE;
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
  auto in = read_inode(iid);
  size_t readed = 0;
  while (readed < size) {
    size_t cur = offset + readed;
    size_t next_boundary = (cur + BLOCK_SIZE) / BLOCK_SIZE * BLOCK_SIZE;
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
  assert(size + (offset % BLOCK_SIZE) <= BLOCK_SIZE);
  auto slbs = sbm_->readSuperBlock()->s_log_block_size_;
  int used_block = in.i_blocks_ / (2 << slbs);
  size_t bid = offset / BLOCK_SIZE;
  size_t local_offset = offset % BLOCK_SIZE;
  if (bid < NDIRECT_BLOCK) {
    assert(in.i_block_[bid]);
    assert(bm_->state(in.i_block_[bid]));
    auto block = bd_->bread(in.i_block_[bid]);
    size_t read_size = std::min(size, BLOCK_SIZE - local_offset);
    memcpy(dst, block->s_ + local_offset, read_size);
    return read_size;
  } else if (bid < NDIRECT_BLOCK +
                       N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t))) {
    assert(in.i_block_[NDIRECT_BLOCK]);
    assert(bm_->state(in.i_block_[NDIRECT_BLOCK]));
    auto b1 = bd_->bread(in.i_block_[NDIRECT_BLOCK]);
    auto local_bid = bid - NDIRECT_BLOCK;
    auto bid1 = local_bid;
    assert(*((uint32_t*)b1->s_ + bid1));
    assert(bm_->state(*((uint32_t*)b1->s_ + bid1)));
    auto block = bd_->bread(*((uint32_t*)b1->s_ + bid1));
    size_t read_size = std::min(size, BLOCK_SIZE - local_offset);
    memcpy(dst, block->s_ + local_offset, read_size);
    return read_size;
  } else if (bid < NDIRECT_BLOCK +
                       N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) +
                       N2INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) *
                           (BLOCK_SIZE / sizeof(uint32_t))) {
    assert(in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK]);
    assert(bm_->state(in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK]));
    auto b1 = bd_->bread(in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK]);
    auto local_bid = bid - NDIRECT_BLOCK -
                     N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t));
    auto bid1 = local_bid / (BLOCK_SIZE / sizeof(uint32_t));
    auto bid2 = local_bid % (BLOCK_SIZE / sizeof(uint32_t));
    assert(*((uint32_t*)b1->s_ + bid1));
    assert(bm_->state(*((uint32_t*)b1->s_ + bid1)));
    auto b2 = bd_->bread(*((uint32_t*)b1->s_ + bid1));
    assert(*((uint32_t*)b2->s_ + bid2));
    assert(bm_->state(*((uint32_t*)b2->s_ + bid2)));
    auto block = bd_->bread(*((uint32_t*)b2->s_ + bid2));

    size_t read_size = std::min(size, BLOCK_SIZE - local_offset);
    memcpy(dst, block->s_ + local_offset, read_size);
    return read_size;
  } else if (bid < NDIRECT_BLOCK +
                       N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) +
                       N2INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) *
                           (BLOCK_SIZE / sizeof(uint32_t)) +
                       N3INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) *
                           (BLOCK_SIZE / sizeof(uint32_t)) *
                           (BLOCK_SIZE / sizeof(uint32_t))) {
    assert(in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK]);
    assert(bm_->state(
        in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK]));
    auto b1 = bd_->bread(
        in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK]);
    auto local_bid = bid - NDIRECT_BLOCK -
                     N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) -
                     N2INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t) *
                                         (BLOCK_SIZE / sizeof(uint32_t)));
    auto bid1 = local_bid / (BLOCK_SIZE / sizeof(uint32_t)) /
                (BLOCK_SIZE / sizeof(uint32_t));
    auto bid2 = local_bid / (BLOCK_SIZE / sizeof(uint32_t)) %
                (BLOCK_SIZE / sizeof(uint32_t));
    auto bid3 = local_bid % (BLOCK_SIZE / sizeof(uint32_t));
    assert(*((uint32_t*)b1->s_ + bid1));
    assert(bm_->state(*((uint32_t*)b1->s_ + bid1)));
    auto b2 = bd_->bread(*((uint32_t*)b1->s_ + bid1));
    assert(*((uint32_t*)b2->s_ + bid2));
    assert(bm_->state(*((uint32_t*)b2->s_ + bid2)));
    auto b3 = bd_->bread(*((uint32_t*)b2->s_ + bid2));
    assert(*((uint32_t*)b3->s_ + bid3));
    assert(bm_->state(*((uint32_t*)b3->s_ + bid3)));
    auto block = bd_->bread(*((uint32_t*)b3->s_ + bid3));
    size_t read_size = std::min(size, BLOCK_SIZE - local_offset);
    memcpy(dst, block->s_ + local_offset, read_size);
    return read_size;
  } else {
    assert(0);
  }
}
size_t InodeManager::write_inode_data_helper(const inode& in, void* src,
                                             size_t offset, size_t size) const {
  assert(size + (offset % BLOCK_SIZE) <= BLOCK_SIZE);
  auto slbs = sbm_->readSuperBlock()->s_log_block_size_;
  int used_block = in.i_blocks_ / (2 << slbs);
  size_t bid = offset / BLOCK_SIZE;
  size_t local_offset = offset % BLOCK_SIZE;
  if (bid < NDIRECT_BLOCK) {
    assert(in.i_block_[bid]);
    assert(bm_->state(in.i_block_[bid]));
    auto block = bd_->bread(in.i_block_[bid]);
    size_t write_size = std::min(size, BLOCK_SIZE - local_offset);
    memcpy(block->s_ + local_offset, src, write_size);
    bd_->bwrite(block.get());
    return write_size;
  } else if (bid < NDIRECT_BLOCK +
                       N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t))) {
    assert(in.i_block_[NDIRECT_BLOCK]);
    assert(bm_->state(in.i_block_[NDIRECT_BLOCK]));
    auto b1 = bd_->bread(in.i_block_[NDIRECT_BLOCK]);
    auto local_bid = bid - NDIRECT_BLOCK;
    auto bid1 = local_bid;
    assert(*((uint32_t*)b1->s_ + bid1));
    assert(bm_->state(*((uint32_t*)b1->s_ + bid1)));
    auto block = bd_->bread(*((uint32_t*)b1->s_ + bid1));
    size_t write_size = std::min(size, BLOCK_SIZE - local_offset);
    memcpy(block->s_ + local_offset, src, write_size);
    bd_->bwrite(block.get());
    return write_size;
  } else if (bid < NDIRECT_BLOCK +
                       N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) +
                       N2INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) *
                           (BLOCK_SIZE / sizeof(uint32_t))) {
    assert(in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK]);
    assert(bm_->state(in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK]));
    auto b1 = bd_->bread(in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK]);
    auto local_bid = bid - NDIRECT_BLOCK -
                     N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t));
    auto bid1 = local_bid / (BLOCK_SIZE / sizeof(uint32_t));
    auto bid2 = local_bid % (BLOCK_SIZE / sizeof(uint32_t));
    assert(*((uint32_t*)b1->s_ + bid1));
    assert(bm_->state(*((uint32_t*)b1->s_ + bid1)));
    auto b2 = bd_->bread(*((uint32_t*)b1->s_ + bid1));
    assert(*((uint32_t*)b2->s_ + bid2));
    assert(bm_->state(*((uint32_t*)b2->s_ + bid2)));
    auto block = bd_->bread(*((uint32_t*)b2->s_ + bid2));

    size_t write_size = std::min(size, BLOCK_SIZE - local_offset);
    memcpy(block->s_ + local_offset, src, write_size);
    bd_->bwrite(block.get());
    return write_size;
  } else if (bid < NDIRECT_BLOCK +
                       N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) +
                       N2INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) *
                           (BLOCK_SIZE / sizeof(uint32_t)) +
                       N3INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) *
                           (BLOCK_SIZE / sizeof(uint32_t)) *
                           (BLOCK_SIZE / sizeof(uint32_t))) {
    assert(in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK]);
    assert(bm_->state(
        in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK]));
    auto b1 = bd_->bread(
        in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK]);
    auto local_bid = bid - NDIRECT_BLOCK -
                     N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) -
                     N2INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t) *
                                         (BLOCK_SIZE / sizeof(uint32_t)));
    auto bid1 = local_bid / (BLOCK_SIZE / sizeof(uint32_t)) /
                (BLOCK_SIZE / sizeof(uint32_t));
    auto bid2 = local_bid / (BLOCK_SIZE / sizeof(uint32_t)) %
                (BLOCK_SIZE / sizeof(uint32_t));
    auto bid3 = local_bid % (BLOCK_SIZE / sizeof(uint32_t));
    assert(*((uint32_t*)b1->s_ + bid1));
    assert(bm_->state(*((uint32_t*)b1->s_ + bid1)));
    auto b2 = bd_->bread(*((uint32_t*)b1->s_ + bid1));
    assert(*((uint32_t*)b2->s_ + bid2));
    assert(bm_->state(*((uint32_t*)b2->s_ + bid2)));
    auto b3 = bd_->bread(*((uint32_t*)b2->s_ + bid2));
    assert(*((uint32_t*)b3->s_ + bid3));
    assert(bm_->state(*((uint32_t*)b3->s_ + bid3)));
    auto block = bd_->bread(*((uint32_t*)b3->s_ + bid3));
    size_t write_size = std::min(size, BLOCK_SIZE - local_offset);
    memcpy(block->s_ + local_offset, src, write_size);
    bd_->bwrite(block.get());
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
// BlockManager
BlockManager::BlockManager(std::shared_ptr<MyDisk> bd,
                           std::shared_ptr<SuperBlockManager> sbm)
    : bd_(bd), sbm_(sbm) {}

bool BlockManager::tagBlock(uint32_t index, bool val) {
  assert(index >= 0 && index < BLOCK_NUM &&
         "Index out of range");  // 确保索引在合法范围内
  int byteIndex = index / 8;
  int bitIndex = index % 8;
  auto block = bd_->bread(3);
  if (val)
    block->s_[byteIndex] |= val << bitIndex;
  else {
    if ((block->s_[byteIndex] >> bitIndex) & 1) remainBlock++;
    block->s_[byteIndex] &= ~(1 << bitIndex);
  }
#ifndef DEPLOY
  if (val) {
    std::cout << "Allocating block " << index << std::endl;
  } else {
    std::cout << "Deallocating block " << index << std::endl;
  }
#endif
  bd_->bwrite(block.get());
  return true;
}

inline bool BlockManager::state(uint32_t index) const {
  assert(index >= 0 && index < BLOCK_NUM &&
         "Index out of range");  // 确保索引在合法范围内
  int byteIndex = index / 8;
  int bitIndex = index % 8;
  auto block = bd_->bread(3);
  return (block->s_[byteIndex] >> bitIndex) & 1;
}

uint32_t BlockManager::getBlock() {
  auto block = bd_->bread(3);
  for (int i = 0; i < BLOCK_NUM; i++) {
    int byteIndex = i / 8;
    int bitIndex = i % 8;
    if (((block->s_[byteIndex] >> bitIndex) & 1) == 0) {
      block->s_[byteIndex] |= 1 << bitIndex;
      // wipe
      auto assigned = bd_->bread(i);
      memset(assigned->s_, 0, BLOCK_SIZE);
      bd_->bwrite(assigned.get());
      //
      remainBlock--;
#ifndef DEPLOY
      std::cout << "Allocating block " << i << std::endl;
#endif
      bd_->bwrite(block.get());
      return i;
    }
  }
  assert(0);
}
bool InodeManager::dir_add_dentry(uint32_t dst, uint32_t src,
                                  const std::string& name) {
  auto in = read_inode(dst);
  assert(in.i_mode_ & EXT2_S_IFDIR);

  // Construct dentry
  assert(name.size() < 256);
  dentry d = {src, static_cast<uint16_t>(UPPER4(sizeof(dentry) + name.size())),
              static_cast<uint8_t>(name.size()), EXT2_FT_REG_FILE};
  if (in.i_size_ % BLOCK_SIZE + d.rec_len_ > BLOCK_SIZE) {
    // modify the rec len of the last dentry
    auto prev = dentry_begin(&in);
    for (auto it = dentry_begin(&in); it != dentry_end(&in); ++it) {
      prev = it;
    }
    auto prev_d = prev.cur_dentry();
    auto new_rec_len = BLOCK_SIZE - prev.offset_ % BLOCK_SIZE;
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

bool FloppyDisk::readdir(const std::string& dir, inode* in,
                         uint32_t* iid) const {
  if (dir.empty()) {
    if (in) *in = im_->read_inode(1);
    if (iid) *iid = 1;
    return true;
  }
  auto path = splitPath(dir);
  assert(path.size() && path[0].size() == 0);
  uint32_t cur_dir_iid{1};
  auto cur_inode = im_->read_inode(cur_dir_iid);
  for (int i = 1; i < path.size(); i++) {
    const auto& match_name = path[i];
    assert(i == path.size() - 1 || cur_inode.i_mode_ & EXT2_S_IFDIR);
    if (!im_->find_next(cur_inode, match_name, &cur_dir_iid)) {
      return false;
    }
    cur_inode = im_->read_inode(cur_dir_iid);
  }
  if (in) *in = cur_inode;
  if (iid) *iid = cur_dir_iid;
  return true;
}

void InodeManager::resize(int iid, uint32_t size) {
  auto inode = read_inode(iid);
  auto slbs = sbm_->readSuperBlock()->s_log_block_size_;
  int used_block = inode.i_blocks_ / (2 << slbs);

  int after_block = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;

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
          used_block - NDIRECT_BLOCK >
                  N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t))
              ? N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t))
              : used_block - NDIRECT_BLOCK;
      free_indirect_blocks(inode.i_block_[NDIRECT_BLOCK], 1, start, end);
      if (after_block <= NDIRECT_BLOCK) {
        bm_->tagBlock(inode.i_block_[NDIRECT_BLOCK], 0);
        inode.i_block_[NDIRECT_BLOCK] = 0;
      }
    }

    // Free double indirect blocks
    if (used_block >
        NDIRECT_BLOCK + N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t))) {
      unsigned int start =
          after_block > NDIRECT_BLOCK +
                            N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t))
              ? after_block -
                    (NDIRECT_BLOCK +
                     N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)))
              : 0;
      unsigned int end =
          used_block - (NDIRECT_BLOCK +
                        N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t))) >
                  N2INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) *
                      (BLOCK_SIZE / sizeof(uint32_t))
              ? N2INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) *
                    (BLOCK_SIZE / sizeof(uint32_t))
              : used_block -
                    (NDIRECT_BLOCK +
                     N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)));

      free_indirect_blocks(inode.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK], 2,
                           start, end);
      if (after_block <=
          NDIRECT_BLOCK + N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t))) {
        bm_->tagBlock(inode.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK], 0);
        inode.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK] = 0;
      }
    }

    // Free triple indirect blocks
    if (used_block > NDIRECT_BLOCK +
                         N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) +
                         N2INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) *
                             (BLOCK_SIZE / sizeof(uint32_t))) {
      unsigned int start =
          after_block > NDIRECT_BLOCK +
                            N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) +
                            N2INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) *
                                (BLOCK_SIZE / sizeof(uint32_t))
              ? after_block -
                    (NDIRECT_BLOCK +
                     N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) +
                     N2INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) *
                         (BLOCK_SIZE / sizeof(uint32_t)))
              : 0;
      unsigned int end =
          used_block - (NDIRECT_BLOCK +
                        N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) +
                        N2INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) *
                            (BLOCK_SIZE / sizeof(uint32_t))) >
                  N3INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) *
                      (BLOCK_SIZE / sizeof(uint32_t)) *
                      (BLOCK_SIZE / sizeof(uint32_t))
              ? N3INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) *
                    (BLOCK_SIZE / sizeof(uint32_t)) *
                    (BLOCK_SIZE / sizeof(uint32_t))
              : used_block -
                    (NDIRECT_BLOCK +
                     N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) +
                     N2INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) *
                         (BLOCK_SIZE / sizeof(uint32_t)));
      free_indirect_blocks(
          inode.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK],
          3, start, end);
      if (after_block <=
          NDIRECT_BLOCK + N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) +
              N2INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) *
                  (BLOCK_SIZE / sizeof(uint32_t))) {
        bm_->tagBlock(
            inode.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK],
            0);
        inode.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK] = 0;
      }
    }
  } else {
    for (int i = used_block; i < after_block; i++) {
      allocate_data(inode, i);
    }
  }
  inode.i_size_ = size;
  inode.i_blocks_ = (size + BLOCK_SIZE - 1) / BLOCK_SIZE * (BLOCK_SIZE / 512);
  write_inode(inode, iid);
#ifndef DEPLOY
  std::cout << "After resize: " << bm_->remainBlock << std::endl;
#endif
}

void InodeManager::free_indirect_blocks(uint32_t bid, int level, size_t start,
                                        size_t end) {
  auto block = bd_->bread(bid);
  if (level == 1) {
    for (size_t i = start; i < end; i++) {
      auto entry = *((uint32_t*)block->s_ + i);
      assert(entry);
      bm_->tagBlock(entry, 0);
      *((uint32_t*)block->s_ + i) = 0;
    }
    if (start == 0) bm_->tagBlock(bid, 0);
    return;
  }

  int level_entries = 1;
  for (int i = 1; i < level; ++i) {
    level_entries *= BLOCK_SIZE / sizeof(uint32_t);
  }

  size_t start_index = start / level_entries;
  size_t end_index = end / level_entries;
  unsigned int sub_start, sub_end;

  for (size_t i = start_index; i <= end_index; ++i) {
    auto entry = *((uint32_t*)block->s_ + i);
    assert(entry);
    sub_start = (i == start_index) ? start % level_entries : 0;
    sub_end = (i == end_index) ? end % level_entries : level_entries;
    free_indirect_blocks(entry, level - 1, sub_start, sub_end);
  }
  if (start_index == 0) bm_->tagBlock(bid, 0);
}

void InodeManager::allocate_data(inode& in, size_t bid) {
  if (bid < NDIRECT_BLOCK) {
    in.i_block_[bid] = bm_->getBlock();
    return;
  } else if (bid < NDIRECT_BLOCK +
                       N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t))) {
    std::unique_ptr<block> b1;
    if (in.i_block_[NDIRECT_BLOCK] == 0) {
      in.i_block_[NDIRECT_BLOCK] = bm_->getBlock();
      b1 = bd_->bread(in.i_block_[NDIRECT_BLOCK]);
      memset(b1->s_, 0, BLOCK_SIZE);
    } else {
      b1 = bd_->bread(in.i_block_[NDIRECT_BLOCK]);
    }
    auto local_bid = bid - NDIRECT_BLOCK;
    auto bid1 = local_bid;
    *((uint32_t*)b1->s_ + bid1) = bm_->getBlock();
    bd_->bwrite(b1.get());
    return;
  } else if (bid < NDIRECT_BLOCK +
                       N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) +
                       N2INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) *
                           (BLOCK_SIZE / sizeof(uint32_t))) {
    std::unique_ptr<block> b1;
    bool modified = false;
    if (in.i_block_[N1INDIRECT_BLOCK + NDIRECT_BLOCK] == 0) {
      in.i_block_[N1INDIRECT_BLOCK + NDIRECT_BLOCK] = bm_->getBlock();
      b1 = bd_->bread(in.i_block_[N1INDIRECT_BLOCK + NDIRECT_BLOCK]);
      memset(b1->s_, 0, BLOCK_SIZE);
      modified |= true;
    } else {
      b1 = bd_->bread(in.i_block_[N1INDIRECT_BLOCK + NDIRECT_BLOCK]);
    }
    auto local_bid = bid - NDIRECT_BLOCK -
                     N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t));
    auto bid1 = local_bid / (BLOCK_SIZE / sizeof(uint32_t));
    auto bid2 = local_bid % (BLOCK_SIZE / sizeof(uint32_t));
    std::unique_ptr<block> b2;
    if (*((uint32_t*)b1->s_ + bid1) == 0) {
      *((uint32_t*)b1->s_ + bid1) = bm_->getBlock();
      b2 = bd_->bread(*((uint32_t*)b1->s_ + bid1));
      memset(b2->s_, 0, BLOCK_SIZE);
      modified |= true;
    } else {
      b2 = bd_->bread(*((uint32_t*)b1->s_ + bid1));
    }
    if (modified) bd_->bwrite(b1.get());
    *((uint32_t*)b2->s_ + bid2) = bm_->getBlock();
    bd_->bwrite(b2.get());
    return;
  } else if (bid < NDIRECT_BLOCK +
                       N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) +
                       N2INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) *
                           (BLOCK_SIZE / sizeof(uint32_t)) +
                       N3INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) *
                           (BLOCK_SIZE / sizeof(uint32_t)) *
                           (BLOCK_SIZE / sizeof(uint32_t))) {
    std::unique_ptr<block> b1;
    bool modified = false;
    if (in.i_block_[N1INDIRECT_BLOCK + N2INDIRECT_BLOCK + NDIRECT_BLOCK] == 0) {
      in.i_block_[N1INDIRECT_BLOCK + N2INDIRECT_BLOCK + NDIRECT_BLOCK] =
          bm_->getBlock();
      b1 = bd_->bread(
          in.i_block_[N1INDIRECT_BLOCK + N2INDIRECT_BLOCK + NDIRECT_BLOCK]);
      memset(b1->s_, 0, BLOCK_SIZE);
      modified |= true;
    } else {
      b1 = bd_->bread(
          in.i_block_[N1INDIRECT_BLOCK + N2INDIRECT_BLOCK + NDIRECT_BLOCK]);
    }
    auto local_bid = bid - NDIRECT_BLOCK -
                     N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t)) -
                     N2INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t) *
                                         (BLOCK_SIZE / sizeof(uint32_t)));
    auto bid1 = local_bid / (BLOCK_SIZE / sizeof(uint32_t)) /
                (BLOCK_SIZE / sizeof(uint32_t));
    auto bid2 = local_bid / (BLOCK_SIZE / sizeof(uint32_t)) %
                (BLOCK_SIZE / sizeof(uint32_t));
    auto bid3 = local_bid % (BLOCK_SIZE / sizeof(uint32_t));
    std::unique_ptr<block> b2;
    if (*((uint32_t*)b1->s_ + bid1) == 0) {
      *((uint32_t*)b1->s_ + bid1) = bm_->getBlock();
      b2 = bd_->bread(*((uint32_t*)b1->s_ + bid1));
      memset(b2->s_, 0, BLOCK_SIZE);
      modified |= true;
    } else {
      b2 = bd_->bread(*((uint32_t*)b1->s_ + bid1));
    }
    if (modified) {
      bd_->bwrite(b1.get());
      modified = false;
    }
    std::unique_ptr<block> b3;
    if (*((uint32_t*)b2->s_ + bid2) == 0) {
      *((uint32_t*)b2->s_ + bid2) = bm_->getBlock();
      b3 = bd_->bread(*((uint32_t*)b2->s_ + bid2));
      memset(b3->s_, 0, BLOCK_SIZE);
      modified |= true;
    } else {
      b3 = bd_->bread(*((uint32_t*)b2->s_ + bid2));
    }
    if (modified) bd_->bwrite(b2.get());
    *((uint32_t*)b3->s_ + bid3) = bm_->getBlock();
    bd_->bwrite(b3.get());
    return;
  }
}
// int FloppyDisk::readtest(const std::string& dir, char* buf, size_t size,
//                          off_t offset) {
//   auto inode = readdir(dir);
//   auto block = bd_->bread(inode.i_block_[0]);
//   memcpy(buf, block->s_, BLOCK_SIZE);
//   return BLOCK_SIZE;
// }

std::unique_ptr<FloppyDisk> FloppyDisk::skipInit() {
  auto my_fs = std::make_unique<FloppyDisk>("/home/iamswing/myfs/simdisk.img");
  return my_fs;
}

std::unique_ptr<FloppyDisk> FloppyDisk::mytest() {
  auto my_fs = std::make_unique<FloppyDisk>("/home/iamswing/myfs/simdisk.img");
  my_fs->initialize();
  return my_fs;
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

int FloppyDisk::read(const std::string& dir, char* buf, size_t size,
                     uint64_t offset) const {
  inode inode;
  uint32_t iid;
  if (!readdir(dir, &inode, &iid)) return -ENOENT;
  if (offset >= inode.i_size_) return 0;
  if (offset + size > inode.i_size_) size = inode.i_size_ - offset;
  return im_->read_inode_data(iid, buf, offset, size);
}

int FloppyDisk::write(const std::string& dir, const char* buf, size_t size,
                      off_t offset) {
  inode inode;
  uint32_t iid;
  if (!readdir(dir, &inode, &iid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFREG)) return -EISDIR;
  if (offset + size > inode.i_size_) im_->resize(iid, offset + size);
  return im_->write_inode_data(iid, buf, offset, size);
}

int FloppyDisk::truncate(const std::string& dir, uint64_t size) {
  inode inode;
  uint32_t iid;
  if (!readdir(dir, &inode, &iid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFREG)) return -EISDIR;
  im_->resize(iid, size);
  return 0;
}

int FloppyDisk::mkdir(const std::string& dir, const inode& in) {
  inode inode;
  uint32_t iid;
  if (readdir(dir, &inode, &iid)) return -EEXIST;

  auto [pName, cName] = splitPathParent(dir);
  if (!readdir(pName, &inode, &iid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;
  inode.i_links_count_++;
  im_->write_inode(inode, iid);

  uint32_t new_iid = im_->new_inode(in);
  im_->dir_add_dentry(new_iid, new_iid, ".");
  im_->dir_add_dentry(new_iid, iid, "..");
  im_->dir_add_dentry(iid, new_iid, cName);
  return 0;
}

int FloppyDisk::create(const std::string& dir, const inode& in) {
  inode inode;
  uint32_t iid;
  if (readdir(dir, &inode, &iid)) return -EEXIST;

  auto [pName, cName] = splitPathParent(dir);
  if (!readdir(pName, &inode, &iid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;

  inode.i_links_count_++;
  im_->write_inode(inode, iid);
  uint32_t new_iid = im_->new_inode(in);
  im_->dir_add_dentry(iid, new_iid, cName);
  return 0;
}

int FloppyDisk::unlink(const std::string& dir) {
  inode pnode, cnode;
  uint32_t piid, ciid;
  if (!readdir(dir, &cnode, &ciid)) return -ENOENT;
  if (!(cnode.i_mode_ & EXT2_S_IFREG)) return -EISDIR;

  auto [pName, cName] = splitPathParent(dir);
  if (!readdir(pName, &pnode, &piid)) return -ENOENT;
  if (!(pnode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;

  im_->dir_del_dentry(piid, cName);
  cnode.i_links_count_--;
  if (cnode.i_links_count_ == 0) {
    im_->resize(ciid, 0);
    im_->del_inode(ciid);
  } else
    im_->write_inode(cnode, ciid);
  return 0;
}

int FloppyDisk::rename(const std::string& oldDir, const std::string& newDir) {
  inode inode;
  uint32_t old_piid, new_piid, ciid;
  if (!readdir(oldDir, &inode, &ciid)) return -ENOENT;

  auto [pName, cName] = splitPathParent(oldDir);
  if (!readdir(pName, nullptr, &old_piid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;

  auto [npName, ncName] = splitPathParent(newDir);
  if (!readdir(npName, nullptr, &new_piid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;

  im_->dir_del_dentry(old_piid, cName);
  if (readdir(newDir, nullptr, &ciid)) im_->dir_del_dentry(new_piid, ncName);
  im_->dir_add_dentry(new_piid, ciid, ncName);
  return 0;
}

int FloppyDisk::rmdir(const std::string& dir) {
  inode pnode, cnode;
  uint32_t piid, ciid;
  if (!readdir(dir, &cnode, &ciid)) return -ENOENT;
  if (!(cnode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;
  if (!im_->dir_empty(ciid)) return -ENOTEMPTY;

  auto [pName, cName] = splitPathParent(dir);
  if (!readdir(pName, &pnode, &piid)) return -ENOENT;
  if (!(pnode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;

  im_->dir_del_dentry(piid, cName);
  im_->resize(ciid, 0);
  im_->del_inode(ciid);
  return 0;
}

#ifndef FUSING
#include <iostream>

int main() {
  auto fs = FloppyDisk::mytest();
  inode in;
  memset(&in, 0, sizeof(inode));
  in.i_mode_ = EXT2_S_IFREG | 0755;
  in.i_links_count_ = 1;
  fs->create("/test", in);
  fs->truncate("/test", 1024 * 1024);
  fs->truncate("/test", 0);
  return 0;
}
#endif