#include "device.h"

#include <iostream>

#include "cassert"
#include "cstring"

MyDisk::MyDisk(const std::string& filename) {
  // open file
  FILE* f = fopen(filename.c_str(), "r");
  assert(f != nullptr);
  // read blocks
  for (int i = 0; i < BLOCK_NUM; i++) {
    fread(blocks_[i].s_, BLOCK_SIZE, 1, f);
    blocks_[i].blockNo_ = i;
  }
  fclose(f);
}

MyDisk::~MyDisk() {}

struct block* MyDisk::bread(int blockNo) {
  assert(blockNo >= 0 && blockNo < BLOCK_NUM);
  return &blocks_[blockNo];
}

bool MyDisk::bwrite(struct block* b) {
  assert(b != nullptr);
  assert(b->blockNo_ >= 0 && b->blockNo_ < BLOCK_NUM);
  FILE* f = fopen("disk", "w");
  assert(f != nullptr);
  // write blocks
  return fwrite(blocks_[b->blockNo_].s_, BLOCK_SIZE, 1, f) == 1 ? true : false;
}

int main() {
  MyDisk disk("disk");
  auto b = disk.bread(0);
  std::string str("i am testing bwrite");
  memcpy(b->s_, str.c_str(), str.size() + 1);
  disk.bwrite(b);
  return 0;
}