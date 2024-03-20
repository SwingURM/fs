#include <cstdint>
#include <string>
constexpr int BLOCK_SIZE = 1024;
constexpr int BLOCK_NUM = 1440;

struct block {
  int blockNo_;
  char s_[BLOCK_SIZE];
};

class blockDevice {
 public:
  virtual struct block* bread(int blockNo) = 0;

  virtual bool bwrite(struct block* b) = 0;
  blockDevice(){};
  virtual ~blockDevice() = default;
};

class MyDisk : blockDevice {
 public:
  MyDisk(const std::string& filename);

  bool bwrite(struct block* b) override;
  struct block* bread(int blockNo) override;

  ~MyDisk();

 private:
  struct block blocks_[BLOCK_NUM];
};
