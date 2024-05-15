#include <memory>

#include "device.h"
#include "ext2.h"

class FloppyDisk {
 public:
  FloppyDisk(std::shared_ptr<MyDisk> bd);
  FloppyDisk(const std::string& filename);
  ~FloppyDisk();

  void initialize();

  SuperBlock readSuperBlock();
  bool writeSuperBlock(SuperBlock sb);

 private:
  std::shared_ptr<MyDisk> bd_;
  SuperBlock sb_;
};