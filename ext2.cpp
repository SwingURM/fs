#include "ext2.h"

#include <cassert>

bool Block_Bitmap::setBit(int index, bool val) {
  assert(index >= 0 && index < BLOCK_SIZE * 8 &&
         "Index out of range");  // 确保索引在合法范围内
  int byteIndex = index / 8;
  int bitIndex = index % 8;
  s_[byteIndex] &= ~(val << bitIndex);
}
int Block_Bitmap::getBit(int index) const {
  assert(index >= 0 && index < BLOCK_SIZE * 8 && "Index out of range");
  int byteIndex = index / 8;
  int bitIndex = index % 8;
  return (s_[byteIndex] >> bitIndex) & 1;
}