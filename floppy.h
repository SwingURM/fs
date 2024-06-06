#include "InodeManager.h"

class FloppyDisk {
 public:
  FloppyDisk(std::shared_ptr<MyDisk> bd);
  FloppyDisk(const std::string& filename);

  void initialize();
  void initializeFloppy();

  bool readdir(const std::string& dir, inode* in = nullptr,
               uint32_t* iid = nullptr) const;

  int rmdir(const std::string& path);
  int rename(const std::string& oldpath, const std::string& newpath);
  int unlink(const std::string& path);
  int mkdir(const std::string& path, const inode& in);
  int create(const std::string& path, const inode& in);
  int truncate(const std::string& path, uint64_t size);
  int read(const std::string& path, char* buf, size_t size,
           uint64_t offset) const;
  int write(const std::string& path, const char* buf, size_t size,
            off_t offset);

  // test
  static std::unique_ptr<FloppyDisk> mytest();
  static std::unique_ptr<FloppyDisk> skipInit();

  std::shared_ptr<SuperBlockManager> sbm_;
  std::shared_ptr<BlockManager> bm_;
  std::shared_ptr<InodeManager> im_;
};