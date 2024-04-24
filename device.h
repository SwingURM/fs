#include <cstdint>
#include <fstream>
#include <string>
constexpr int BLOCK_SIZE = 1024;
constexpr int BLOCK_NUM = 1440;

struct block {
  int blockNo_;
  char s_[BLOCK_SIZE];
};

class BlockDevice {
 public:
  virtual struct block* bread(int blockNo) = 0;

  virtual bool bwrite(const block& b) = 0;
  BlockDevice(){};
  virtual ~BlockDevice() = default;
};

class MyDisk : BlockDevice {
 public:
  MyDisk(const std::string& filename);
  ~MyDisk();

  bool initialize(bool format = false);
  bool bwrite(const block& b) override;
  struct block* bread(int blockNo) override;

 private:
  std::fstream file_;
  std::string filename_;
};
