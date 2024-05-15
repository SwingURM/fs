#include "floppy.h"

#include <cstring>
#include <ctime>
#include <iostream>

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
  sb.s_blocks_count_ = 1439;
  // sb_.s_r_blocks_count_ = ;
  // sb_.s_free_blocks_count_ = 1439;
  // sb_.s_free_inodes_count_ = ;
  sb.s_first_data_block_ = 1;
  // sb_.s_log_block_size_ = 0;
  // sb_.s_log_frag_size_ = ;
  sb.s_blocks_per_group_ = 1439;
  // sb_.s_frags_per_group_ = ;
  sb.s_inodes_per_group_ =
      sb.s_blocks_per_group_ * (BLOCK_SIZE / sizeof(inode));
  sb.s_mtime_ = time(nullptr);
  sb.s_wtime_ = time(nullptr);
  // sb_.s_mnt_count_ = ;
  // sb_.s_max_mnt_count_ = ;
  sb.s_magic_ = EXT2_SUPER_MAGIC;
  // TODO 假设这里是挂载
  sb.s_state_ = EXT2_ERROR_FS;

  // Initialize block group descriptor table

  // Initialize block bitmap

  // Initialize inode bitmap

  // Initialize inode table
  writeSuperBlock(sb);
}

FloppyDisk::~FloppyDisk() {
  struct block b;
  b.blockNo_ = 1;
  memcpy(b.s_, &sb_, sizeof(SuperBlock));
  bd_->bwrite(b);
}

bool FloppyDisk::writeSuperBlock(SuperBlock sb) {
  block bl;
  bl.blockNo_ = 1;
  memcpy(bl.s_, &sb, sizeof(SuperBlock));
  bd_->bwrite(bl);
  return true;
}

SuperBlock FloppyDisk::readSuperBlock() {
  auto sb = bd_->bread(1);
  SuperBlock ret;
  memcpy(&ret, sb->s_, sizeof(SuperBlock));
  delete sb;
  return ret;
}

int main() {
  auto bd = std::make_shared<MyDisk>("simdisk.img");
  FloppyDisk fs(bd);

  fs.initialize();

  struct block b;
  b.blockNo_ = 10;
  std::string str("i am testing bwrite");
  memcpy(b.s_, str.c_str(), str.size() + 1);
  bd->bwrite(b);

  // 从第一个块读取数据
  auto readData = bd->bread(10);
  std::cout << readData->s_ << std::endl;

  std::cout << sizeof(inode) << std::endl;
  return 0;
}