#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "floppy.h"

std::unique_ptr<FloppyDisk> my_fs = nullptr;

static void *my_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
  (void)conn;
  (void)cfg;
  my_fs = std::move(FloppyDisk::mytest());

  return nullptr;
}

static int my_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
  (void)fi;
  assert(my_fs);
  std::cout << "reading" << std::endl;
  int res = my_fs->readtest(path, buf, size, offset);
  if (res < 0) return -errno;
  return res;
}
static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi,
                         enum fuse_readdir_flags flags) {
  (void)offset;
  (void)fi;
  (void)flags;
  assert(my_fs);
  if (strcmp(path, "/") != 0) return -ENOENT;
  filler(buf, ".", NULL, 0, FUSE_FILL_DIR_PLUS);
  filler(buf, "..", NULL, 0, FUSE_FILL_DIR_PLUS);
  return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi) {
  assert(my_fs);
  return 0;
}

static int hello_getattr(const char *path, struct stat *stbuf,
                         struct fuse_file_info *fi) {
  (void)fi;
  assert(my_fs);
  std::cout << "Getting attr Path:" << path << std::endl;
  if (strcmp(path, "/") != 0) return -ENOENT;
  memset(stbuf, 0, sizeof(struct stat));
  uint32_t iid;
  auto inode = my_fs->readdir(path, &iid);
  memset(stbuf, 0, sizeof(struct stat));
  stbuf->st_ino = iid;
  stbuf->st_mode = inode.i_mode_;
  return 0;
}
// 其他FUSE回调函数，可以类似实现

static struct fuse_operations my_oper = {
    .getattr = hello_getattr,
    .open = hello_open,
    .read = my_read,
    .readdir = hello_readdir,
    .init = my_init,
    // 其他操作函数可以在这里添加
};

int main(int argc, char *argv[]) {
  return fuse_main(argc, argv, &my_oper, nullptr);
}
