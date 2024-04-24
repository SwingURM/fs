#include <cstring>
#include <iostream>

#include "device.h"
int main() {
  MyDisk bd("simdisk.img");

  if (!bd.initialize(true)) {  // 初始化并格式化磁盘
    return -1;
  }
  struct block b;
  b.blockNo_ = 0;
  std::string str("i am testing bwrite");
  memcpy(b.s_, str.c_str(), str.size() + 1);
  bd.bwrite(b);

  // 从第一个块读取数据
  auto readData = bd.bread(0);
  std::cout << readData->s_ << std::endl;
  return 0;
}
