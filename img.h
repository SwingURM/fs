#include "device.h"
class ImgMaker {
 public:
  static void initFloppy(std::shared_ptr<MyDisk> bd);
  static void initFloppyPlus(std::shared_ptr<MyDisk> bd);
};