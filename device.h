#pragma once
#include <fstream>
#include <memory>
#include <string>
constexpr int BLOCK_SIZE = 1024;
constexpr int BLOCK_NUM = 32768;

struct DeviceBlock {
  char s_[BLOCK_SIZE];
};

class MyDisk {
 public:
  MyDisk(const std::string& filename);
  ~MyDisk();

  bool initialize(bool format = false);
  bool bwrite(const DeviceBlock* b, int blockNo);
  std::unique_ptr<DeviceBlock> bread(int blockNo);

 private:
  std::fstream file_;
  std::string filename_;
};
