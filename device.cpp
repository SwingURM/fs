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
    DeviceBlock emptyBlock;
    memset(emptyBlock.s_, 0, BLOCK_SIZE);
    for (int i = 0; i < BLOCK_NUM; i++) {
      bwrite(&emptyBlock, i);
    }
  }
  return true;
}

bool MyDisk::bwrite(const DeviceBlock* b, int blockNo) {
  assert(blockNo >= 0 && blockNo < BLOCK_NUM);
  assert(file_.is_open());
  file_.seekp(blockNo * BLOCK_SIZE);
  file_.write(b->s_, BLOCK_SIZE);
  return true;
}
std::unique_ptr<DeviceBlock> MyDisk::bread(int blockNo) {
  assert(blockNo >= 0 && blockNo < BLOCK_NUM);
  assert(file_.is_open());
  auto b = std::make_unique<DeviceBlock>();
  file_.seekg(blockNo * BLOCK_SIZE);
  file_.read(b->s_, BLOCK_SIZE);
  return b;
}