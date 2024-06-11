#include "floppy.h"

#include <cassert>
#include <cstring>

#include "img.h"
#include "util.h"

MyFS::MyFS(std::shared_ptr<MyDisk> bd) {
  assert(bd->initialize());
  sbm_ = std::make_shared<SuperBlockManager>(bd);
  bm_ = std::make_shared<BlockManager>(bd, sbm_);
  im_ = std::make_shared<InodeManager>(sbm_, bm_);
}
MyFS::MyFS(const std::string& filename) {
  auto bd = std::make_shared<MyDisk>(filename);
  assert(bd->initialize());
  sbm_ = std::make_shared<SuperBlockManager>(bd);
  bm_ = std::make_shared<BlockManager>(bd, sbm_);
  im_ = std::make_shared<InodeManager>(sbm_, bm_);
}

bool MyFS::readdir(const std::string& dir, inode* in, uint32_t* iid) const {
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

std::unique_ptr<MyFS> MyFS::skipInit() {
  auto my_fs = std::make_unique<MyFS>("/home/iamswing/myfs/simdisk.img");
  return my_fs;
}

std::unique_ptr<MyFS> MyFS::mytest() {
  auto bd = std::make_shared<MyDisk>("/home/iamswing/myfs/simdisk.img");
  assert(bd->initialize(true));
  ImgMaker::initFloppyPlus(bd);
  return std::make_unique<MyFS>(bd);
}

int MyFS::read(const std::string& dir, char* buf, size_t size,
               uint64_t offset) const {
  inode inode;
  uint32_t iid;
  if (!readdir(dir, &inode, &iid)) return -ENOENT;
  if (offset >= inode.i_size_) return 0;
  if (offset + size > inode.i_size_) size = inode.i_size_ - offset;
  return im_->read_inode_data(iid, buf, offset, size);
}

int MyFS::write(const std::string& dir, const char* buf, size_t size,
                off_t offset) {
  inode inode;
  uint32_t iid;
  if (!readdir(dir, &inode, &iid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFREG)) return -EISDIR;
  if (offset + size > inode.i_size_) im_->resize(iid, offset + size);
  return im_->write_inode_data(iid, buf, offset, size);
}

int MyFS::truncate(const std::string& dir, uint64_t size) {
  inode inode;
  uint32_t iid;
  if (!readdir(dir, &inode, &iid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFREG)) return -EISDIR;
  im_->resize(iid, size);
  return 0;
}

int MyFS::mkdir(const std::string& dir, const inode& in) {
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

int MyFS::create(const std::string& dir, const inode& in) {
  inode inode;
  uint32_t iid;
  if (readdir(dir, &inode, &iid)) return -EEXIST;

  auto [pName, cName] = splitPathParent(dir);
  if (!readdir(pName, &inode, &iid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;

  uint32_t new_iid = im_->new_inode(in);
  im_->dir_add_dentry(iid, new_iid, cName);
  return 0;
}

int MyFS::unlink(const std::string& dir) {
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

int MyFS::rename(const std::string& oldDir, const std::string& newDir,
                 unsigned int flags) {
  //   inode inode;
  uint32_t old_ciid{}, new_ciid{}, old_piid{}, new_piid{};
  inode old_inode, new_inode;
  bool old_exist = readdir(oldDir, &old_inode, &old_ciid);
  bool new_exist = readdir(newDir, &new_inode, &new_ciid);
  if (!old_exist) return -ENOENT;
  if (flags & RENAME_EXCHANGE) {
    if (!new_exist) return -ENOENT;
    im_->write_inode(old_inode, new_ciid);
    im_->write_inode(new_inode, old_ciid);
    return 0;
  }
  auto [pName, cName] = splitPathParent(oldDir);
  auto [npName, ncName] = splitPathParent(newDir);
  assert(readdir(pName, nullptr, &old_piid));
  assert(readdir(npName, nullptr, &new_piid));
  if (new_exist && new_inode.i_mode_ & EXT2_S_IFDIR) {
    if (old_inode.i_mode_ & EXT2_S_IFDIR) {
      // both src and dst are dir
      im_->dir_del_dentry(old_piid, cName);
      im_->dir_add_dentry(new_ciid, old_ciid, cName);
      auto pinode = im_->read_inode(old_piid);
      pinode.i_links_count_--;
      im_->write_inode(pinode, old_piid);
      pinode = im_->read_inode(new_ciid);
      pinode.i_links_count_++;
      im_->write_inode(pinode, new_ciid);
      return 0;
    }
    // src is file, dst is dir

    im_->dir_del_dentry(old_piid, cName);
    im_->dir_add_dentry(new_piid, old_ciid, cName);
    return 0;
  }
  if (new_exist && new_inode.i_mode_ & EXT2_S_IFREG &&
      old_inode.i_mode_ & EXT2_S_IFDIR)
    // src is dir, dst is file
    return -EISDIR;
  if (new_exist && new_inode.i_mode_ & EXT2_S_IFREG) {
    // src is file, dst is file
    if (flags & RENAME_NOREPLACE) return -EEXIST;
    unlink(newDir);
    im_->dir_del_dentry(old_piid, cName);
    im_->dir_add_dentry(new_piid, old_ciid, cName);
    return 0;
  }
  assert(!new_exist);
  if (old_inode.i_mode_ & EXT2_S_IFDIR) {
    // src is dir, dst not exist
    im_->dir_del_dentry(old_piid, cName);
    im_->dir_add_dentry(new_piid, old_ciid, ncName);
    //
    auto pinode = im_->read_inode(old_piid);
    pinode.i_links_count_--;
    im_->write_inode(pinode, old_piid);
    //
    pinode = im_->read_inode(new_piid);
    pinode.i_links_count_++;
    im_->write_inode(pinode, new_piid);
  } else {
    // src is file, dst not exist
    im_->dir_del_dentry(old_piid, cName);
    im_->dir_add_dentry(new_piid, old_ciid, ncName);
  }
  return 0;
}

int MyFS::rmdir(const std::string& dir) {
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
  auto fs = MyFS::mytest();
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