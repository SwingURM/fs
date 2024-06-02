#include <memory>
#include <vector>

#include "ext2.h"
class SuperBlockManager {
 public:
  SuperBlockManager(std::shared_ptr<MyDisk>);
  std::unique_ptr<SuperBlock> readSuperBlock();
  bool writeSuperBlock(const SuperBlock* const sb);

 private:
  std::shared_ptr<MyDisk> bd_;
};
class BlockManager {
 public:
  BlockManager(std::shared_ptr<MyDisk>, std::shared_ptr<SuperBlockManager>);
  bool tagBlock(uint32_t index, bool val);
  bool getBlock() const;

 private:
  std::shared_ptr<MyDisk> bd_;
  std::shared_ptr<SuperBlockManager> sbm_;
};
class InodeManager {
 public:
  InodeManager(std::shared_ptr<MyDisk>, std::shared_ptr<SuperBlockManager>,
               std::shared_ptr<BlockManager>);
  uint32_t new_inode();
  inode read_inode(int iid) const;
  size_t read_inode_data(const inode& in, void* dst, size_t offset,
                         size_t size) const;
  bool write_inode(const inode& in, int iid);
  /**
   * @brief Find the dentry with the given name in the given directory.
   *
   * @param in The inode of the given directory.
   * @param dir The name of the dentry to be found.
   * @param ret The inode ID of the dentry to be found.
   */
  bool find_next(const inode& in, const std::string& dir,
                 uint32_t* ret = nullptr);
  /**
   * @brief Add a directory entry to a directory inode.
   *
   * @param in The inode to be modified
   * @param iid The inode ID of the dentry to be added.
   * @param name The name of the dentry to be added.
   */
  bool dir_add_dentry(uint32_t dst, uint32_t src, const std::string& name);
  class dentry_iterator {
   public:
    dentry_iterator(const inode& in, InodeManager* im, size_t offset = 0);

    dentry_iterator& operator++();
    dentry current_dentry() const;
    std::string cur_dentry_name() const;
    friend bool operator==(const dentry_iterator& lhs,
                           const dentry_iterator& rhs);
    friend bool operator!=(const dentry_iterator& lhs,
                           const dentry_iterator& rhs);

   private:
    inode dinode_;
    InodeManager* im_;
    size_t offset_;
  };
  dentry_iterator dentry_begin(const inode& in);
  dentry_iterator dentry_end(const inode& in);

 private:
  std::shared_ptr<MyDisk> bd_;
  std::shared_ptr<BlockManager> bm_;
  std::shared_ptr<SuperBlockManager> sbm_;
};

class BGDMangaer {};

class FloppyDisk {
 public:
  FloppyDisk(std::shared_ptr<MyDisk> bd);
  FloppyDisk(const std::string& filename);

  void initialize();

  // dir
  bool readdir(const std::string& dir, inode* in = nullptr,
               uint32_t* iid = nullptr) const;
  //   int readtest(const std::string& dir, char* buf, size_t size, off_t
  //   offset);

  // test
  static std::unique_ptr<FloppyDisk> mytest();

  std::shared_ptr<MyDisk> bd_;
  std::shared_ptr<SuperBlockManager> sbm_;
  std::shared_ptr<BlockManager> bm_;
  std::shared_ptr<InodeManager> im_;

 private:
  std::vector<std::string> splitPath(const std::string& path) const;
};