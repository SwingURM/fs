#pragma once
#include <fstream>
#include <string>
constexpr int BLOCK_SIZE = 1024;
constexpr int BLOCK_NUM = 1440;

struct block {
  int blockNo_;
  char s_[BLOCK_SIZE];
};

class MyDisk {
 public:
  MyDisk(const std::string& filename);
  ~MyDisk();

  bool initialize(bool format = false);
  bool bwrite(const block& b) ;
  struct block* bread(int blockNo) ;

 private:
  std::fstream file_;
  std::string filename_;
};
