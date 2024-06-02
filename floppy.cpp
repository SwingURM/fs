#include "floppy.h"

#include <cassert>
#include <cstring>
#include <ctime>
#include <iostream>
#include <sstream>
SuperBlockManager::SuperBlockManager(std::shared_ptr<MyDisk> bd) : bd_(bd) {}

std::unique_ptr<SuperBlock> SuperBlockManager::readSuperBlock() {
  assert(BLOCK_SIZE == 1024);
  auto sb = bd_->bread(1);
  SuperBlock* ret = new SuperBlock;
  memcpy(ret, sb->s_, sizeof(SuperBlock));
  return std::unique_ptr<SuperBlock>(ret);
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
  for (int i = 0; i < 28; i++) {
    // TODO: OPTIMIZE
    bm_->tagBlock(i, 1);
  }

  // Initialize inode bitmap
  auto inode_map_block = bd_->bread(4);
  memset(inode_map_block->s_, 0, BLOCK_SIZE);
  bd_->bwrite(inode_map_block.get());
  auto iid = im_->new_inode();
  assert(iid == 1);

  // Initialize inode table
  inode root_inode;
  root_inode.i_mode_ = EXT2_S_IFDIR;
  root_inode.i_size_ = 0;
  root_inode.i_blocks_ = 0;

  im_->write_inode(root_inode, 1);
}

// InodeManager
InodeManager::InodeManager(std::shared_ptr<MyDisk> bd,
                           std::shared_ptr<SuperBlockManager> sbm,
                           std::shared_ptr<BlockManager> bm)
    : bd_(bd), sbm_(sbm), bm_(bm) {}

uint32_t InodeManager::new_inode() {
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

  // Construct inode
  inode in;
  in.i_size_ = 0;
  in.i_blocks_ = 0;

  auto sb = sbm_->readSuperBlock();
  int block_group = (iid - 1) / sb->s_inodes_per_group_;
  assert(block_group == 0);
  int local_inode_index = (iid - 1) % sb->s_inodes_per_group_;
  assert(local_inode_index == local_iid);

  // edit inode table
  auto inode_tbl_block = bd_->bread(5);
  auto inode_tbl = (inode_table*)inode_tbl_block->s_;
  inode_tbl->inodes_[local_inode_index] = in;
  bd_->bwrite(inode_tbl_block.get());
  return iid;
}

inode InodeManager::read_inode(int iid) const {
  auto sb = sbm_->readSuperBlock();
  int block_group = (iid - 1) / sb->s_inodes_per_group_;
  assert(block_group == 0);
  int local_inode_index = (iid - 1) % sb->s_inodes_per_group_;
  assert(local_inode_index < sb->s_inodes_per_group_);

  int block_group_bid = block_group * sb->s_blocks_per_group_ + 1;
  // TODO locating by bg_inode_table
  int inode_table_bid = block_group_bid + 4;
  auto bp = bd_->bread(inode_table_bid);
  auto tp = (inode_table*)bp->s_;
  return tp->inodes_[local_inode_index];
}

bool InodeManager::write_inode(const inode& in, int iid) {
  auto sb = sbm_->readSuperBlock();
  int block_group = (iid - 1) / sb->s_inodes_per_group_;
  assert(block_group == 0);
  int local_inode_index = (iid - 1) % sb->s_inodes_per_group_;
  assert(local_inode_index < sb->s_inodes_per_group_);

  int block_group_bid = block_group * sb->s_blocks_per_group_ + 1;
  // TODO locating by bg_inode_table
  int inode_table_bid = block_group_bid + 4;
  auto bp = bd_->bread(inode_table_bid);
  auto tp = (inode_table*)bp->s_;
  tp->inodes_[local_inode_index] = in;
  bd_->bwrite(bp.get());
  return true;
}

size_t InodeManager::read_inode_data(const inode& in, void* dst, size_t offset,
                                     size_t size) const {
  auto slbs = sbm_->readSuperBlock()->s_log_block_size_;
  int used_block = in.i_blocks_ / (2 << slbs);
  size_t bid = offset / BLOCK_SIZE;
  size_t local_offset = offset % BLOCK_SIZE;
  if (bid < NDIRECT_BLOCK) {
    auto block = bd_->bread(in.i_block_[bid]);
    size_t read_size = std::min(size, BLOCK_SIZE - local_offset);
    memcpy(dst, block->s_ + local_offset, read_size);
    return read_size;
  } else if (bid < NDIRECT_BLOCK + N1INDIRECT_BLOCK) {
    auto b1 = bd_->bread(in.i_block_[NDIRECT_BLOCK]);
    auto local_bid = bid - NDIRECT_BLOCK;
    auto block = bd_->bread(*((uint32_t*)b1->s_ + local_bid));
    size_t read_size = std::min(size, BLOCK_SIZE - local_offset);
    memcpy(dst, block->s_ + local_offset, read_size);
    return read_size;
  } else if (bid < NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK) {
    auto b1 = bd_->bread(in.i_block_[NDIRECT_BLOCK + N1INDIRECT_BLOCK]);
    auto local_bid = bid - NDIRECT_BLOCK -
                     N1INDIRECT_BLOCK * (BLOCK_SIZE / sizeof(uint32_t));
    auto bid1 = local_bid / (BLOCK_SIZE / sizeof(uint32_t));
    auto bid2 = local_bid % (BLOCK_SIZE / sizeof(uint32_t));
    auto b2 = bd_->bread(*((uint32_t*)b1->s_ + bid1));
    auto block = bd_->bread(*((uint32_t*)b2->s_ + bid2));

    size_t read_size = std::min(size, BLOCK_SIZE - local_offset);
    memcpy(dst, block->s_ + local_offset, read_size);
    return read_size;
  } else if (bid < NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK +
                       N3INDIRECT_BLOCK) {
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
    auto b2 = bd_->bread(*((uint32_t*)b1->s_ + bid1));
    auto b3 = bd_->bread(*((uint32_t*)b2->s_ + bid2));
    auto block = bd_->bread(*((uint32_t*)b3->s_ + bid3));
    size_t read_size = std::min(size, BLOCK_SIZE - local_offset);
    memcpy(dst, block->s_ + local_offset, read_size);
    return read_size;
  } else {
    assert(0);
  }
}
bool InodeManager::find_next(const inode& in, const std::string& dir,
                             uint32_t* ret) {
  for (auto it = dentry_begin(in); it != dentry_end(in); ++it) {
    if (it.cur_dentry_name() == dir) {
      *ret = it.current_dentry().inode_;
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
  assert(index >= 0 && index < BLOCK_SIZE * 8 &&
         "Index out of range");  // 确保索引在合法范围内
  int byteIndex = index / 8;
  int bitIndex = index % 8;
  auto block = bd_->bread(3);
  block->s_[byteIndex] |= val << bitIndex;
  bd_->bwrite(block.get());
  return true;
}

bool BlockManager::getBlock() const {
  auto block = bd_->bread(3);
  for (int i = 0; i < BLOCK_SIZE * 8; i++) {
    int byteIndex = i / 8;
    int bitIndex = i % 8;
    if (((block->s_[byteIndex] >> bitIndex) & 1) == 0) {
      block->s_[byteIndex] &= ~(1 << bitIndex);
      bd_->bwrite(block.get());
      return i;
    }
  }
  assert(0);
}
bool InodeManager::dir_add_dentry(uint32_t dst, uint32_t src,
                                  const std::string& name) {
  auto in = read_inode(dst);
  assert(in.i_mode_ == EXT2_S_IFDIR);
  assert(name.size() < 256);
  dentry d = {src, static_cast<uint16_t>(UPPER4(sizeof(dentry) + name.size())),
              static_cast<uint8_t>(name.size()), EXT2_FT_REG_FILE};
  auto slbs = sbm_->readSuperBlock()->s_log_block_size_;
  int used_block = in.i_blocks_ / (2 << slbs);
  assert(used_block <= 1);
  if (used_block == 0) {
    auto bid = bm_->getBlock();
    in.i_blocks_ = BLOCK_SIZE / 512;
    in.i_block_[0] = bid;
  }
  auto block = bd_->bread(in.i_block_[0]);
  assert(in.i_size_ % 4 == 0 && in.i_size_ + d.rec_len_ <= BLOCK_SIZE);
  dentry* ptr = (dentry*)(block->s_ + in.i_size_);
  *ptr = d;
  memcpy(block->s_ + in.i_size_ + sizeof(d), name.c_str(), name.size());
  in.i_size_ += d.rec_len_;
  bd_->bwrite(block.get());
  write_inode(in, dst);
  return true;
}

std::vector<std::string> FloppyDisk::splitPath(const std::string& path) const {
  char delimiter = '/';
  std::vector<std::string> result;
  std::istringstream iss(path);
  std::string token;

  while (std::getline(iss, token, delimiter)) {
    result.push_back(token);
  }

  return result;
}

bool FloppyDisk::readdir(const std::string& dir, inode* in,
                         uint32_t* iid) const {
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

// int FloppyDisk::readtest(const std::string& dir, char* buf, size_t size,
//                          off_t offset) {
//   auto inode = readdir(dir);
//   auto block = bd_->bread(inode.i_block_[0]);
//   memcpy(buf, block->s_, BLOCK_SIZE);
//   return BLOCK_SIZE;
// }

std::unique_ptr<FloppyDisk> FloppyDisk::mytest() {
  auto my_fs = std::make_unique<FloppyDisk>("/home/iamswing/myfs/simdisk.img");
  my_fs->initialize();
  uint32_t iid;
  inode inode;
  iid = my_fs->im_->new_inode();
  assert(iid == 2);
  inode.i_mode_ = EXT2_S_IFREG;
  inode.i_size_ = 0;
  inode.i_blocks_ = 0;
  my_fs->im_->write_inode(inode, iid);

  assert(my_fs->readdir("/", &inode, &iid));
  assert(iid == 1);

  my_fs->im_->dir_add_dentry(1, 2, "fuck");
  my_fs->im_->dir_add_dentry(1, 2, "qwq");
  my_fs->im_->dir_add_dentry(1, 2, "zzz");
  my_fs->im_->dir_add_dentry(1, 1, ".");
  my_fs->im_->dir_add_dentry(1, 1, "..");

  return my_fs;
}

InodeManager::dentry_iterator::dentry_iterator(const inode& in,
                                               InodeManager* im, size_t offset)
    : dinode_(in), im_(im), offset_(offset) {}

dentry InodeManager::dentry_iterator::current_dentry() const {
  assert(offset_ != dinode_.i_size_);
  dentry ret;
  auto read_size = im_->read_inode_data(dinode_, &ret, offset_, sizeof(dentry));
  if (read_size < sizeof(dentry)) {
    char* offset = (char*)&ret + read_size;
    auto read_size2 = im_->read_inode_data(dinode_, offset, offset_ + read_size,
                                           sizeof(dentry) - read_size);
    assert(read_size + read_size2 == sizeof(dentry));
  }
  return ret;
}
std::string InodeManager::dentry_iterator::cur_dentry_name() const {
  assert(offset_ != dinode_.i_size_);
  auto dentry = current_dentry();
  char name[256];
  auto read_size = im_->read_inode_data(dinode_, name, offset_ + sizeof(dentry),
                                        dentry.name_len_);
  if (read_size < dentry.name_len_) {
    auto read_size2 = im_->read_inode_data(dinode_, name + read_size,
                                           offset_ + sizeof(dentry) + read_size,
                                           dentry.name_len_ - read_size);
    assert(read_size + read_size2 == dentry.name_len_);
  }
  return std::string(name, dentry.name_len_);
}
InodeManager::dentry_iterator& InodeManager::dentry_iterator::operator++() {
  assert(offset_ != dinode_.i_size_);
  auto dentry = current_dentry();
  offset_ += dentry.rec_len_;
  return *this;
}

InodeManager::dentry_iterator InodeManager::dentry_begin(const inode& in) {
  return InodeManager::dentry_iterator(in, this, 0);
}
InodeManager::dentry_iterator InodeManager::dentry_end(const inode& in) {
  return InodeManager::dentry_iterator(in, this, in.i_size_);
}

bool operator==(const InodeManager::dentry_iterator& lhs,
                const InodeManager::dentry_iterator& rhs) {
  if (memcmp(&lhs.dinode_, &rhs.dinode_, sizeof(inode)) != 0) {
    return false;
  }
  return lhs.offset_ == rhs.offset_;
}

bool operator!=(const InodeManager::dentry_iterator& lhs,
                const InodeManager::dentry_iterator& rhs) {
  if (memcmp(&lhs.dinode_, &rhs.dinode_, sizeof(inode)) != 0) {
    return true;
  }
  return lhs.offset_ != rhs.offset_;
}

// int main() {
//   auto fs = FloppyDisk::mytest();
//   uint32_t iid;
//   fs->readdir("/", nullptr, &iid);
//   std::cout << iid << std::endl;
//   fs->readdir("/fuck", nullptr, &iid);
//   std::cout << iid << std::endl;

//   return 0;
// }