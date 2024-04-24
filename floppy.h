#include "ext2.h"
#include "device.h"

class FloppyDisk {
 public:
  FloppyDisk(const std::string& filename);
  ~FloppyDisk();

  void initialize();
  

 private:
  MyDisk bd_;
  SuperBlock sb_;
};