#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>

#include "floppy.h"
#include "util.h"

std::unique_ptr<FloppyDisk> my_fs = nullptr;
std::mutex my_mutex;

static void *my_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
  (void)conn;
  (void)cfg;
  my_fs = std::move(FloppyDisk::mytest());

  return nullptr;
}

static int my_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
  (void)fi;
  std::lock_guard<std::mutex> guard(my_mutex);
  assert(my_fs);
  inode inode;
  uint32_t iid;
  if (!my_fs->readdir(path, &inode, &iid)) return -ENOENT;
  if (offset >= inode.i_size_) return 0;
  if (offset + size > inode.i_size_) size = inode.i_size_ - offset;

  auto res = my_fs->im_->read_inode_data(iid, buf, offset, size);
  if (res < 0) return -errno;
  return res;
}

static int my_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
  std::lock_guard<std::mutex> guard(my_mutex);
  inode inode;
  uint32_t iid;
  if (!my_fs->readdir(path, &inode, &iid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFREG)) return -EISDIR;

  if (offset + size > inode.i_size_) my_fs->im_->resize(iid, offset + size);
  return my_fs->im_->write_inode_data(iid, buf, offset, size);
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi,
                         enum fuse_readdir_flags flags) {
  (void)offset;
  (void)fi;
  (void)flags;
  std::lock_guard<std::mutex> guard(my_mutex);
  assert(my_fs);
  inode inode;
  if (!my_fs->readdir(path, &inode)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;
  for (auto iter = my_fs->im_->dentry_begin(&inode);
       iter != my_fs->im_->dentry_end(&inode); ++iter) {
    std::string name = iter.cur_dentry_name();
    filler(buf, name.c_str(), NULL, 0, FUSE_FILL_DIR_PLUS);
  }
  return 0;
}

static int my_truncate(const char *path, off_t size,
                       struct fuse_file_info *fi) {
  std::lock_guard<std::mutex> guard(my_mutex);
  uint32_t iid;
  inode inode;
  if (!my_fs->readdir(path, &inode, &iid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFREG)) return -EISDIR;

  my_fs->im_->resize(iid, size);
  return 0;
}

static int my_mkdir(const char *path, mode_t mode) {
  std::lock_guard<std::mutex> guard(my_mutex);
  inode inode;
  uint32_t iid;
  auto [parentName, dirname] = splitPathParent(path);

  if (!my_fs->readdir(parentName, &inode, &iid)) return -ENOENT;

  if (!(inode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;

  uint32_t new_iid = my_fs->im_->new_inode();
  memset(&inode, 0, sizeof(inode));
  inode.i_mode_ = EXT2_S_IFDIR;
  inode.i_links_count_ = 1;
  my_fs->im_->write_inode(inode, new_iid);
  my_fs->im_->dir_add_dentry(new_iid, new_iid, ".");
  my_fs->im_->dir_add_dentry(new_iid, iid, "..");

  my_fs->im_->dir_add_dentry(iid, new_iid, dirname);

  return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi) {
  std::lock_guard<std::mutex> guard(my_mutex);
  if (!my_fs->readdir(path)) {
    return -ENOENT;
  }
  return 0;
}

static int my_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
  std::lock_guard<std::mutex> guard(my_mutex);
  inode inode;
  uint32_t iid;
  auto [parentName, childName] = splitPathParent(path);
  if (!my_fs->readdir(parentName, &inode, &iid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;

  uint32_t new_iid = my_fs->im_->new_inode();
  memset(&inode, 0, sizeof(inode));
  inode.i_mode_ = EXT2_S_IFREG;
  inode.i_links_count_ = 1;
  my_fs->im_->write_inode(inode, new_iid);
  my_fs->im_->dir_add_dentry(iid, new_iid, childName);
  return 0;
}

static int my_unlink(const char *path) {
  std::lock_guard<std::mutex> guard(my_mutex);
  inode inode;
  uint32_t iid, child_iid;
  auto [parentName, childName] = splitPathParent(path);
  if (!my_fs->readdir(parentName, &inode, &iid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;
  if (!my_fs->readdir(path, &inode, &child_iid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFREG)) return -EISDIR;
  my_fs->im_->dir_del_dentry(iid, childName);
  inode.i_links_count_--;
  if (inode.i_links_count_ == 0) my_fs->im_->del_inode(child_iid);
  else my_fs->im_->write_inode(inode, child_iid);
  return 0;
}

static int my_rmdir(const char *path) {
  std::lock_guard<std::mutex> guard(my_mutex);
  inode inode;
  uint32_t iid;
  auto [parentName, dirname] = splitPathParent(path);
  if (!my_fs->readdir(parentName, &inode, &iid)) return -ENOENT;
  auto parent_iid = iid;

  if (!(inode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;

  if (!my_fs->readdir(path, &inode, &iid)) return -ENOENT;
  if (!(inode.i_mode_ & EXT2_S_IFDIR)) return -ENOTDIR;
  if (!my_fs->im_->dir_empty(iid)) return -ENOTEMPTY;

  my_fs->im_->dir_del_dentry(parent_iid, dirname);
  inode.i_links_count_--;
  my_fs->im_->write_inode(inode, iid);
  return 0;
}

static int hello_getattr(const char *path, struct stat *stbuf,
                         struct fuse_file_info *fi) {
  (void)fi;
  std::lock_guard<std::mutex> guard(my_mutex);
  assert(my_fs);
  uint32_t iid;
  inode inode;
  if (!my_fs->readdir(path, &inode, &iid)) return -ENOENT;
  memset(stbuf, 0, sizeof(struct stat));
  stbuf->st_ino = iid;
  stbuf->st_mode = inode.i_mode_;
  stbuf->st_blocks = inode.i_blocks_;
  stbuf->st_size = inode.i_size_;
  return 0;
}
// 其他FUSE回调函数，可以类似实现

static struct fuse_operations my_oper = {
    .getattr = hello_getattr,
    .mkdir = my_mkdir,
    .unlink = my_unlink,
    .rmdir = my_rmdir,
    .truncate = my_truncate,
    .open = hello_open,
    .read = my_read,
    .write = my_write,
    .readdir = hello_readdir,
    .init = my_init,
    .create = my_create,
};

int main(int argc, char *argv[]) {
  return fuse_main(argc, argv, &my_oper, nullptr);
}
