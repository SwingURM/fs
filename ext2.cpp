#include "ext2.h"

#include <cassert>

bool Block_Bitmap::setBit(int index, bool val) {
  // TODO ASSERT GROUP
  assert(index >= 0 && index < BLOCK_SIZE * 8 &&
         "Index out of range");  // 确保索引在合法范围内
  int byteIndex = index / 8;
  int bitIndex = index % 8;
  s_[byteIndex] &= ~(val << bitIndex);
  return true;
}
int Block_Bitmap::getBit(int index) const {
  // TODO ASSERT GROUP
  assert(index >= 0 && index < BLOCK_SIZE * 8 && "Index out of range");
  int byteIndex = index / 8;
  int bitIndex = index % 8;
  return (s_[byteIndex] >> bitIndex) & 1;
}
int Block_Bitmap::get_idle() const {
  for (int i = 0; i < BLOCK_SIZE * 8; i++) {
    // TODO ASSERT GROUP
    if (getBit(i) == 0) return i;
  }
  assert(0);
}

bool Inode_Bitmap::setBit(int index, bool val) {
  index -= 1;
  assert(index >= 0 && index < BLOCK_SIZE * 8 &&
         "Index out of range");  // 确保索引在合法范围内
  int byteIndex = index / 8;
  int bitIndex = index % 8;
  s_[byteIndex] &= ~(val << bitIndex);
  return true;
}

int Inode_Bitmap::getBit(int index) const {
  index -= 1;
  assert(index >= 0 && index < BLOCK_SIZE * 8);
  int byteIndex = index / 8;
  int bitIndex = index % 8;
  return (s_[byteIndex] >> bitIndex) & 1;
}

int Inode_Bitmap::get_idle() const {
  for (int i = 1; i <= BLOCK_SIZE * 8; i++) {
    if (getBit(i) == 0) return i + 1;
  }
  assert(0);
}

inode inode_table::read_inode(int iid) {
  // local inode index
  return inodes_[iid];
}
bool inode_table::write_inode(inode in, int iid) {
  inodes_[iid] = in;
  return true;
}