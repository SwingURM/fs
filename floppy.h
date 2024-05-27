#include <memory>
#include <vector>

#include "device.h"
#include "ext2.h"

class FloppyDisk {
 public:
  FloppyDisk(std::shared_ptr<MyDisk> bd);
  FloppyDisk(const std::string& filename);
  ~FloppyDisk();

  void initialize();

  std::unique_ptr<SuperBlock> readSuperBlock();
  bool writeSuperBlock(const SuperBlock* const sb);

  uint32_t new_inode(uint16_t mode);
  uint32_t new_data();

  // inode
  inode readinode(int iid);
  bool writeinode(inode in, int iid);

  bool dir_add_dentry(inode& in, uint32_t iid, const std::string& name);

  uint32_t find_next(inode in, const std::string& dir);
  // dir
  inode readdir(const std::string& dir);

 private:
  std::shared_ptr<MyDisk> bd_;
  std::vector<std::string> splitPath(const std::string& path);
};