#include "floppy.h"

#include <cassert>
#include <cstring>

#include "util.h"

FloppyDisk::FloppyDisk(std::shared_ptr<MyDisk> bd) {
  assert(bd->initialize());
  sbm_ = std::make_shared<SuperBlockManager>(bd);
  bm_ = std::make_shared<BlockManager>(bd, sbm_);
  im_ = std::make_shared<InodeManager>(sbm_, bm_);
}
FloppyDisk::FloppyDisk(const std::string& filename) {
  auto bd = std::make_shared<MyDisk>(filename);
  assert(bd->initialize());
  sbm_ = std::make_shared<SuperBlockManager>(bd);
  bm_ = std::make_shared<BlockManager>(bd, sbm_);
  im_ = std::make_shared<InodeManager>(sbm_, bm_);
}

void FloppyDisk::initializeFloppy() {
  SuperBlock sb;
  memset(&sb, 0, sizeof(SuperBlock));
  // Initialize superblock
  //   sb_.s_inodes_count_ = ? ;
  sb.s_blocks_count_ = 1440;
  // sb_.s_r_blocks_count_ = ;
  // sb_.s_free_blocks_count_ = 1439;
  // sb_.s_free_inodes_count_ = ;
  sb.s_first_data_block_ = 1;
  sb.s_log_block_size_ = 0;
  // sb_.s_log_frag_size_ = ;
  sb.s_blocks_per_group_ = 1439;
  // sb_.s_frags_per_group_ = ;
  sb.s_inodes_per_group_ = 23 * (1024 / sizeof(inode));
  sb.s_mtime_ = time(nullptr);
  sb.s_wtime_ = time(nullptr);
  // sb_.s_mnt_count_ = ;
  // sb_.s_max_mnt_count_ = ;
  sb.s_magic_ = EXT2_SUPER_MAGIC;
  // TODO 假设这里是挂载
  sb.s_state_ = EXT2_ERROR_FS;
  sbm_->writeSuperBlock(&sb);

  // Initialize block group descriptor table
  FSBlock blk{std::make_unique<char[]>(1024)};
  auto bgd = reinterpret_cast<Block_Group_Descriptor*>(blk.s_.get());
  bgd[0].bg_block_bitmap_ = 3;
  bgd[0].bg_inode_bitmap_ = 4;
  bgd[0].bg_inode_table_ = 5;
  bm_->writeBlock(blk, 2);

  bm_->refresh();

  // Initialize block bitmap
  memset(blk.s_.get(), 0, 1024);
  bm_->writeBlock(blk, 3);
  for (int i = 1; i < 28; i++) {
    // TODO: OPTIMIZE
    bm_->tagBlock(i, 1);
  }

  // Initialize inode bitmap
  bm_->writeBlock(blk, 4);
  // Initialize inode table
  bm_->writeBlock(blk, 5);

  inode root_inode;
  memset(&root_inode, 0, sizeof(inode));
  root_inode.i_mode_ = EXT2_S_IFDIR;
  root_inode.i_links_count_ = 2;

  //
  auto iid = im_->new_inode(root_inode);
  assert(iid == 1);

  im_->dir_add_dentry(1, 1, ".");
  im_->dir_add_dentry(1, 1, "..");
}

void FloppyDisk::initialize() {
  SuperBlock sb;
  memset(&sb, 0, sizeof(SuperBlock));
  // Initialize superblock
  //   sb_.s_inodes_count_ = ? ;
  sb.s_blocks_count_ = 1440;
  // sb_.s_r_blocks_count_ = ;
  // sb_.s_free_blocks_count_ = 1439;
  // sb_.s_free_inodes_count_ = ;
  sb.s_first_data_block_ = 0;
  sb.s_log_block_size_ = 2;
  // sb_.s_log_frag_size_ = ;
  sb.s_blocks_per_group_ = 1439;
  // sb_.s_frags_per_group_ = ;
  sb.s_inodes_per_group_ = 23 * (4096 / sizeof(inode));
  sb.s_mtime_ = time(nullptr);
  sb.s_wtime_ = time(nullptr);
  // sb_.s_mnt_count_ = ;
  // sb_.s_max_mnt_count_ = ;
  sb.s_magic_ = EXT2_SUPER_MAGIC;
  // TODO 假设这里是挂载
  sb.s_state_ = EXT2_ERROR_FS;
  sbm_->writeSuperBlock(&sb);

  // Initialize block group descriptor table
  FSBlock blk{std::make_unique<char[]>(4096)};
  auto bgd = reinterpret_cast<Block_Group_Descriptor*>(blk.s_.get());
  bgd[0].bg_block_bitmap_ = 2;
  bgd[0].bg_inode_bitmap_ = 3;
  bgd[0].bg_inode_table_ = 4;
  bm_->writeBlock(blk, 1);

  bm_->refresh();

  // Initialize block bitmap
  memset(blk.s_.get(), 0, 4096);
  bm_->writeBlock(blk, 2);
  for (int i = 0; i < 28; i++) {
    // TODO: OPTIMIZE
    bm_->tagBlock(i, 1);
  }

  // Initialize inode bitmap
  bm_->writeBlock(blk, 3);
  // Initialize inode table
  bm_->writeBlock(blk, 4);

  inode root_inode;
  memset(&root_inode, 0, sizeof(inode));
  root_inode.i_mode_ = EXT2_S_IFDIR;
  root_inode.i_links_count_ = 2;

  //
  auto iid = im_->new_inode(root_inode);
  assert(iid == 1);

  im_->dir_add_dentry(1, 1, ".");
  im_->dir_add_dentry(1, 1, "..");
}

bool FloppyDisk::readdir(const std::string& dir, inode* in,
                         uint32_t* iid) const {
  if (dir.empty()) {
    if (in) *in = im_->read_inode(1);
    if (iid) *iid = 1;
    return true;
  }
  auto path = splitPath(dir);
  assert(path.size() && path[0].size() == 0);
  uint32_t cur_dir_iid{1};
  auto cur_inode = im_->read_inode(cur_dir_iid);
  for (int i = 1; i < path.size(); i++) {
    const auto& match_name = path[i];
    assert(i == path.size() - 1 || cur_inode.i_mode_ & EXT2_S_IFDIR);
    if (!im_->find_next(cur_inode, match_name, &cur_dir_iid)) {
      return false;
    }
    cur_inode = im_->read_inode(cur_dir_iid);
  }
  if (in) *in = cur_inode;
  if (iid) *iid = cur_dir_iid;
  return true;
}

std::unique_ptr<FloppyDisk> FloppyDisk::skipInit() {
  auto my_fs = std::make_unique<FloppyDisk>("/home/iamswing/myfs/simdisk.img");
  my_fs->bm_->refresh();
  return my_fs;
}

std::unique_ptr<FloppyDisk> FloppyDisk::mytest() {
  auto my_fs = std::make_unique<FloppyDisk>("/home/iamswing/myfs/simdisk.img");
  my_fs->initializeFloppy();
  return my_fs;
}

int FloppyDisk::read(const std::string& dir, char* buf, size_t size,
                     uint64_t offset) const {
  inode inode;
  uint32_t iid;
  if (!readdir(dir, &inode, &iid)) return -ENOENT;
  if (offset >= inode.i_size_) return 0;
  if (offset + size > inode.i_size_) size = inode.i_size_ - offset;
  return im_->read_inode_data(iid, buf, offset, size);
}

int FloppyDisk::write(const std::string& dir, const char* buf, size_t size,
                      off_t offset) {
  inode inode;
  uint32_t iid;
  if (!readdir(dir, &inode, &iid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFREG)) return -EISDIR;
  if (offset + size > inode.i_size_) im_->resize(iid, offset + size);
  return im_->write_inode_data(iid, buf, offset, size);
}

int FloppyDisk::truncate(const std::string& dir, uint64_t size) {
  inode inode;
  uint32_t iid;
  if (!readdir(dir, &inode, &iid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFREG)) return -EISDIR;
  im_->resize(iid, size);
  return 0;
}

int FloppyDisk::mkdir(const std::string& dir, const inode& in) {
  inode inode;
  uint32_t iid;
  if (readdir(dir, &inode, &iid)) return -EEXIST;

  auto [pName, cName] = splitPathParent(dir);
  if (!readdir(pName, &inode, &iid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;
  inode.i_links_count_++;
  im_->write_inode(inode, iid);

  uint32_t new_iid = im_->new_inode(in);
  im_->dir_add_dentry(new_iid, new_iid, ".");
  im_->dir_add_dentry(new_iid, iid, "..");
  im_->dir_add_dentry(iid, new_iid, cName);
  return 0;
}

int FloppyDisk::create(const std::string& dir, const inode& in) {
  inode inode;
  uint32_t iid;
  if (readdir(dir, &inode, &iid)) return -EEXIST;

  auto [pName, cName] = splitPathParent(dir);
  if (!readdir(pName, &inode, &iid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;

  inode.i_links_count_++;
  im_->write_inode(inode, iid);
  uint32_t new_iid = im_->new_inode(in);
  im_->dir_add_dentry(iid, new_iid, cName);
  return 0;
}

int FloppyDisk::unlink(const std::string& dir) {
  inode pnode, cnode;
  uint32_t piid, ciid;
  if (!readdir(dir, &cnode, &ciid)) return -ENOENT;
  if (!(cnode.i_mode_ & EXT2_S_IFREG)) return -EISDIR;

  auto [pName, cName] = splitPathParent(dir);
  if (!readdir(pName, &pnode, &piid)) return -ENOENT;
  if (!(pnode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;

  im_->dir_del_dentry(piid, cName);
  cnode.i_links_count_--;
  if (cnode.i_links_count_ == 0) {
    im_->resize(ciid, 0);
    im_->del_inode(ciid);
  } else
    im_->write_inode(cnode, ciid);
  return 0;
}

int FloppyDisk::rename(const std::string& oldDir, const std::string& newDir) {
  inode inode;
  uint32_t old_piid, new_piid, ciid;
  if (!readdir(oldDir, &inode, &ciid)) return -ENOENT;

  auto [pName, cName] = splitPathParent(oldDir);
  if (!readdir(pName, nullptr, &old_piid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;

  auto [npName, ncName] = splitPathParent(newDir);
  if (!readdir(npName, nullptr, &new_piid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;

  im_->dir_del_dentry(old_piid, cName);
  if (readdir(newDir, nullptr, &ciid)) im_->dir_del_dentry(new_piid, ncName);
  im_->dir_add_dentry(new_piid, ciid, ncName);
  return 0;
}

int FloppyDisk::rmdir(const std::string& dir) {
  inode pnode, cnode;
  uint32_t piid, ciid;
  if (!readdir(dir, &cnode, &ciid)) return -ENOENT;
  if (!(cnode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;
  if (!im_->dir_empty(ciid)) return -ENOTEMPTY;

  auto [pName, cName] = splitPathParent(dir);
  if (!readdir(pName, &pnode, &piid)) return -ENOENT;
  if (!(pnode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;

  im_->dir_del_dentry(piid, cName);
  im_->resize(ciid, 0);
  im_->del_inode(ciid);
  return 0;
}

#ifndef FUSING
#include <iostream>

int main() {
  auto fs = FloppyDisk::mytest();
  inode in;
  memset(&in, 0, sizeof(inode));
  in.i_mode_ = EXT2_S_IFREG | 0755;
  in.i_links_count_ = 1;
  fs->create("/test", in);
  fs->truncate("/test", 1024 * 1024);
  char buf[1024];
  for (int i = 0; i < 1024; i++) {
    fs->read("/test", buf, 1024, i * 1024);
  }
  fs->truncate("/test", 0);
  return 0;
}
#endif