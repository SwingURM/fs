#include <memory>
#include <vector>

#include "device.h"
#include "ext2.h"

class SuperBlockManager {
 public:
  SuperBlockManager(std::shared_ptr<MyDisk>);
  const SuperBlock& readSuperBlock();
  bool writeSuperBlock(const SuperBlock* const sb);

 private:
  std::shared_ptr<MyDisk> bd_;
  SuperBlock sb_;
};

struct FSBlock {
  std::unique_ptr<char[]> s_;
};

class BGDMangaer {};

class BlockManager {
 public:
  BlockManager(std::shared_ptr<MyDisk>, std::shared_ptr<SuperBlockManager>);

  // didn't check prev value
  bool tagBlock(uint32_t index, bool val);
  uint32_t getIdleBlock();
  bool state(uint32_t index) const;

  FSBlock readBlock(uint32_t bid) const;
  bool writeBlock(const FSBlock& block, uint32_t bid);
  void refresh();

  std::vector<Block_Group_Descriptor> bgd_;

 private:
  std::shared_ptr<MyDisk> bd_;
  std::shared_ptr<SuperBlockManager> sbm_;
};