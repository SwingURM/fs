#include "floppy.h"

#include <cassert>
#include <cstring>
#include <ctime>
#include <iostream>
#include <sstream>

FloppyDisk::FloppyDisk(std::shared_ptr<MyDisk> bd) : bd_(bd) {
  if (!bd_->initialize()) {
    std::cerr << "Unable to initialize floppy disk." << std::endl;
    exit(-1);
  }
}
FloppyDisk::FloppyDisk(const std::string& filename)
    : bd_(std::make_shared<MyDisk>(filename)) {
  if (!bd_->initialize()) {
    std::cerr << "Unable to initialize floppy disk." << std::endl;
    exit(-1);
  }
}

void FloppyDisk::initialize() {
  auto sb = readSuperBlock();
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
  writeSuperBlock(sb.get());

  // Initialize block group descriptor table

  // Initialize block bitmap
  auto block_bitmap_block = bd_->bread(3);
  auto block_bitmap = (Block_Bitmap*)block_bitmap_block.get();
  for (int i = 0; i < 28; i++) {
    block_bitmap->setBit(i, 1);
  }
  bd_->bwrite(block_bitmap_block.get());

  // Initialize inode bitmap
  auto inode_bitmap_block = bd_->bread(4);
  auto inode_bitmap = (Inode_Bitmap*)inode_bitmap_block->s_;
  inode_bitmap->setBit(1, 1);
  bd_->bwrite(inode_bitmap_block.get());

  // Initialize inode table
  inode root_inode;
  root_inode.i_mode_ = EXT2_S_IFDIR;
  root_inode.i_size_ = 0;
  root_inode.i_blocks_ = 0;

  auto inode_tbl_block = bd_->bread(5);
  auto inode_tbl = (inode_table*)inode_tbl_block->s_;
  int block_group = (1 - 1) / sb->s_inodes_per_group_;
  int local_inode_index = (1 - 1) % sb->s_inodes_per_group_;
  inode_tbl->write_inode(root_inode, local_inode_index);
  bd_->bwrite(inode_tbl_block.get());

}

FloppyDisk::~FloppyDisk() {}
uint32_t FloppyDisk::new_inode(uint16_t mode) {
  // TODO: ASSERT inode bitmap
  auto inode_map_block = bd_->bread(4);
  auto inode_map = (Inode_Bitmap*)inode_map_block->s_;
  auto iid = inode_map->get_idle();
  inode_map->setBit(iid, 1);
  bd_->bwrite(inode_map_block.get());
  inode in;
  in.i_mode_ = mode;
  in.i_size_ = 0;
  in.i_blocks_ = 0;
  auto inode_tbl_block = bd_->bread(5);
  auto inode_tbl = (inode_table*)inode_tbl_block->s_;

  auto sb = readSuperBlock();
  int block_group = (iid - 1) / sb->s_inodes_per_group_;
  int local_inode_index = (iid - 1) % sb->s_inodes_per_group_;
  inode_tbl->write_inode(in, local_inode_index);
  bd_->bwrite(inode_tbl_block.get());
  return iid;
}
uint32_t FloppyDisk::new_data() {
  // TODO ASSERT
  auto block_bitmap_block = bd_->bread(3);
  auto block_bitmap = (Block_Bitmap*)block_bitmap_block->s_;
  auto bid = block_bitmap->get_idle();
  block_bitmap->setBit(bid, 1);
  return bid;
}

bool FloppyDisk::writeSuperBlock(const SuperBlock* const sb) {
  block bl;
  memcpy(bl.s_, sb, sizeof(SuperBlock));
  bl.blockNo_ = 1;
  bd_->bwrite(&bl);
  return true;
}

std::unique_ptr<SuperBlock> FloppyDisk::readSuperBlock() {
  auto sb = bd_->bread(1);
  SuperBlock* ret = new SuperBlock;
  memcpy(ret, sb->s_, sizeof(SuperBlock));
  return std::unique_ptr<SuperBlock>(ret);
}
inode FloppyDisk::readinode(int iid) {
  auto sb = readSuperBlock();
  int block_group = (iid - 1) / sb->s_inodes_per_group_;
  int local_inode_index = (iid - 1) % sb->s_inodes_per_group_;

  int block_group_bid = block_group * sb->s_blocks_per_group_ + 1;
  // TODO locating by bg_inode_table
  int inode_table_bid = block_group_bid + 4;
  auto bp = bd_->bread(inode_table_bid);
  auto tp = (inode_table*)bp->s_;
  return tp->read_inode(local_inode_index);
}
bool FloppyDisk::writeinode(inode in, int iid) {
  auto sb = readSuperBlock();
  int block_group = (iid - 1) / sb->s_inodes_per_group_;
  int local_inode_index = (iid - 1) % sb->s_inodes_per_group_;

  int block_group_bid = block_group * sb->s_blocks_per_group_ + 1;
  // TODO locating by bg_inode_table
  int inode_table_bid = block_group_bid + 4;
  auto bp = bd_->bread(inode_table_bid);
  auto tp = (inode_table*)bp->s_;
  tp->write_inode(in, local_inode_index);
  bd_->bwrite(bp.get());
  return true;
}

bool FloppyDisk::dir_add_dentry(inode& in, uint32_t iid,
                                const std::string& name) {
  assert(in.i_mode_ == EXT2_S_IFDIR);
  assert(name.size() < 256);
  dentry d = {iid, static_cast<uint16_t>(UPPER4(sizeof(dentry) + name.size())),
              static_cast<uint8_t>(name.size()), EXT2_FT_REG_FILE};
  auto sb = readSuperBlock();
  int used_block = in.i_blocks_ / (2 << sb->s_log_block_size_);
  assert(used_block <= 1);
  if (used_block == 0) {
    auto bid = new_data();
    in.i_blocks_ = 2;
    in.i_block_[0] = bid;
  }
  auto block = bd_->bread(in.i_block_[0]);
  assert(in.i_size_ % 4 == 0 && in.i_size_ + d.rec_len_ <= BLOCK_SIZE);
  dentry* ptr = (dentry*)(block->s_ + in.i_size_);
  *ptr = d;
  memcpy(block->s_ + in.i_size_ + sizeof(d), name.c_str(), name.size());
  in.i_size_ += d.rec_len_;
  bd_->bwrite(block.get());
  return true;
}

std::vector<std::string> FloppyDisk::splitPath(const std::string& path) {
  char delimiter = '/';
  std::vector<std::string> result;
  std::istringstream iss(path);
  std::string token;

  while (std::getline(iss, token, delimiter)) {
    result.push_back(token);
  }

  return result;
}

uint32_t FloppyDisk::find_next(inode in, const std::string& dir) {
  for (int i = 0; i < NDIRECT_BLOCK; i++) {
    auto bp = bd_->bread(in.i_block_[i]);
    int offset = 0;
    while (offset < BLOCK_SIZE) {
      dentry* entry = (dentry*)(bp->s_ + offset);
      if (dir.size() == entry->name_len_) {
        char* name = bp->s_ + offset + sizeof(dentry);
        if (strncmp(dir.c_str(), name, entry->name_len_) == 0) {
          return entry->inode_;
        }
      }
      offset += entry->rec_len_;
    }
  }
  for (int i = NDIRECT_BLOCK; i < NDIRECT_BLOCK + N1INDIRECT_BLOCK; i++) {
    auto bp = bd_->bread(in.i_block_[i]);
    for (int j = 0; j < BLOCK_SIZE / sizeof(uint32_t); j++) {
      uint32_t bp1_bid = *((uint32_t*)bp->s_ + j);
      auto bp1 = bd_->bread(bp1_bid);
      int offset = 0;
      while (i < BLOCK_SIZE) {
        dentry* entry = (dentry*)(bp1->s_ + offset);
        if (dir.size() == entry->name_len_) {
          char* name = bp1->s_ + offset;
          if (strncmp(dir.c_str(), name, entry->name_len_) == 0) {
            return entry->inode_;
          }
        }
        offset += entry->rec_len_;
      }
    }
  }
  for (int i = NDIRECT_BLOCK + N1INDIRECT_BLOCK;
       i < NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK; i++) {
    auto bp = bd_->bread(in.i_block_[i]);
    for (int j = 0; j < BLOCK_SIZE / sizeof(uint32_t); j++) {
      uint32_t bp1_bid = *((uint32_t*)bp->s_ + j);
      auto bp1 = bd_->bread(bp1_bid);
      for (int k = 0; k < BLOCK_SIZE / sizeof(uint32_t); k++) {
        uint32_t bp2_bid = *((uint32_t*)bp1->s_ + k);
        auto bp2 = bd_->bread(bp2_bid);
        int offset = 0;
        while (i < BLOCK_SIZE) {
          dentry* entry = (dentry*)(bp2->s_ + offset);
          if (dir.size() == entry->name_len_) {
            char* name = bp2->s_ + offset;
            if (strncmp(dir.c_str(), name, entry->name_len_) == 0) {
              return entry->inode_;
            }
          }
          offset += entry->rec_len_;
        }
      }
    }
  }

  for (int i = NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK;
       i <
       NDIRECT_BLOCK + N1INDIRECT_BLOCK + N2INDIRECT_BLOCK + N3INDIRECT_BLOCK;
       i++) {
    auto bp = bd_->bread(in.i_block_[i]);
    for (int j = 0; j < BLOCK_SIZE / sizeof(uint32_t); j++) {
      uint32_t bp1_bid = *((uint32_t*)bp->s_ + j);
      auto bp1 = bd_->bread(bp1_bid);
      for (int k = 0; k < BLOCK_SIZE / sizeof(uint32_t); k++) {
        uint32_t bp2_bid = *((uint32_t*)bp1->s_ + k);
        auto bp2 = bd_->bread(bp2_bid);
        for (int l = 0; l < BLOCK_SIZE / sizeof(uint32_t); l++) {
          uint32_t bp3_bid = *((uint32_t*)bp2->s_ + k);
          auto bp3 = bd_->bread(bp3_bid);
          int offset = 0;
          while (i < BLOCK_SIZE) {
            dentry* entry = (dentry*)(bp3->s_ + offset);
            if (dir.size() == entry->name_len_) {
              char* name = bp3->s_ + offset;
              if (strncmp(dir.c_str(), name, entry->name_len_) == 0) {
                return entry->inode_;
              }
            }
            offset += entry->rec_len_;
          }
        }
      }
    }
  }
  assert(0);
  return -1;
}

inode FloppyDisk::readdir(const std::string& dir) {
  auto path = splitPath(dir);
  assert(path.size() && path[0].size() == 0);
  auto cur_dir_iid = 1;
  auto cur_inode = readinode(cur_dir_iid);
  for (int i = 1; i < path.size(); i++) {
    const auto& match_name = path[i];
    assert(i == path.size() - 1 || cur_inode.i_mode_ & EXT2_S_IFDIR);
    cur_dir_iid = find_next(cur_inode, path[i]);
    cur_inode = readinode(cur_dir_iid);
  }
  return cur_inode;
}

int main() {
  auto bd = std::make_shared<MyDisk>("simdisk.img");
  FloppyDisk fs(bd);

  fs.initialize();

  auto inode = fs.readdir("/");
  auto inode1 = fs.new_inode(EXT2_S_IFDIR);
  fs.dir_add_dentry(inode, inode1, "fuck");
  fs.dir_add_dentry(inode, inode1, "qwq");
  fs.dir_add_dentry(inode, inode1, "zzz");
  fs.writeinode(inode, 1);
  auto inode11 = fs.readdir("/fuck");


  return 0;
}