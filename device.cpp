#include "device.h"

#include <iostream>

#include "cassert"
#include "cstring"

MyDisk::MyDisk(const std::string& filename) : filename_(filename) {}

MyDisk::~MyDisk() {
  if (file_.is_open()) {
    file_.close();
  }
}

bool MyDisk::initialize(bool format) {
  file_.open(filename_, std::ios::in | std::ios::out | std::ios::binary);
  if (!file_.is_open()) {
    std::cerr << "Unable to open file." << std::endl;
    return false;
  }
  if (format) {
    block emptyBlock;
    memset(emptyBlock.s_, '0', BLOCK_SIZE);
    for (int i = 0; i < BLOCK_NUM; i++) {
      emptyBlock.blockNo_ = i;
      bwrite(emptyBlock);
    }
  }
  return true;
}

struct block* MyDisk::bread(int blockNo) {
  assert(blockNo >= 0 && blockNo < BLOCK_NUM);
  assert(file_.is_open());
  block* b = new block;
  b->blockNo_ = blockNo;
  file_.seekg(blockNo * BLOCK_SIZE);
  file_.read(b->s_, BLOCK_SIZE);
  return b;
}

bool MyDisk::bwrite(const struct block& b) {
  assert(b.blockNo_ >= 0 && b.blockNo_ < BLOCK_NUM);
  assert(file_.is_open());
  file_.seekp(b.blockNo_ * BLOCK_SIZE);
  file_.write(b.s_, BLOCK_SIZE);
  return true;
}
