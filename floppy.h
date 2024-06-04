#include <memory>

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
  uint32_t getBlock() const;

 private:
  std::shared_ptr<MyDisk> bd_;
  std::shared_ptr<SuperBlockManager> sbm_;
};
class InodeManager {
 public:
  InodeManager(std::shared_ptr<MyDisk>, std::shared_ptr<SuperBlockManager>,
               std::shared_ptr<BlockManager>);
  uint32_t new_inode();
  inode read_inode(uint32_t iid) const;

  size_t read_inode_data(uint32_t iid, void* dst, size_t offset,
                         size_t size) const;

  size_t write_inode_data(uint32_t iid, const void* src, size_t offset,
                          size_t size) const;

  // didn't writeback inode
  void allocate_data(inode& in, size_t bid);

  bool write_inode(const inode& in, uint32_t iid);
  /**
   * @brief Find the dentry with the given name in the given directory.
   *
   * @param in The inode of the given directory.
   * @param dir The name of the dentry to be found.
   * @param ret The inode ID of the dentry to be found.
   */
  bool find_next(inode in, const std::string& dir, uint32_t* ret = nullptr);
  /**
   * @brief Add a directory entry to a directory inode.
   *
   * @param in The inode to be modified
   * @param iid The inode ID of the dentry to be added.
   * @param name The name of the dentry to be added.
   */
  bool dir_add_dentry(uint32_t dst, uint32_t src, const std::string& name);
  bool dir_del_dentry(uint32_t dst, const std::string& name);
  bool dir_empty(uint32_t dst);

  void resize(int iid, uint32_t size);
  void free_indirect_blocks(uint32_t bid, int level, size_t start, size_t end);

  class dentry_iterator {
   public:
    dentry_iterator(inode* in, InodeManager* im, size_t offset = 0);

    dentry_iterator& operator++();
    dentry cur_dentry() const;
    std::string cur_dentry_name() const;
    friend bool operator==(const dentry_iterator& lhs,
                           const dentry_iterator& rhs);
    friend bool operator!=(const dentry_iterator& lhs,
                           const dentry_iterator& rhs);

    inode* dinode_;
    InodeManager* im_;
    size_t offset_;
  };
  dentry_iterator dentry_begin(inode* in);
  dentry_iterator dentry_end(inode* in);

 private:
  std::shared_ptr<MyDisk> bd_;
  std::shared_ptr<BlockManager> bm_;
  std::shared_ptr<SuperBlockManager> sbm_;
  size_t read_inode_data_helper(const inode& in, void* dst, size_t offset,
                                size_t size) const;
  size_t write_inode_data_helper(const inode& in, void* src, size_t offset,
                                 size_t size) const;
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
  static std::unique_ptr<FloppyDisk> skipInit();

  std::shared_ptr<MyDisk> bd_;
  std::shared_ptr<SuperBlockManager> sbm_;
  std::shared_ptr<BlockManager> bm_;
  std::shared_ptr<InodeManager> im_;
};