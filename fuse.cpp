#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
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
  return my_fs->read(path, buf, size, offset);
}

static int my_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
  std::lock_guard<std::mutex> guard(my_mutex);
  return my_fs->write(path, buf, size, offset);
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
    if (name[0] == '.') continue;
    filler(buf, name.c_str(), NULL, 0, FUSE_FILL_DIR_PLUS);
  }
  return 0;
}

static int my_truncate(const char *path, off_t size,
                       struct fuse_file_info *fi) {
  std::lock_guard<std::mutex> guard(my_mutex);
  return my_fs->truncate(path, size);
}

static int my_mkdir(const char *path, mode_t mode) {
  std::lock_guard<std::mutex> guard(my_mutex);
  inode dinode;
  memset(&dinode, 0, sizeof(inode));
  dinode.i_mode_ = EXT2_S_IFDIR | (mode & 0777);
  dinode.i_uid_ = getuid();
  dinode.i_gid_ = getgid();
  dinode.i_links_count_ = 2;
  dinode.i_atime_ = time(nullptr);
  dinode.i_ctime_ = time(nullptr);
  dinode.i_mtime_ = time(nullptr);

  return my_fs->mkdir(path, dinode);
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
  memset(&inode, 0, sizeof(inode));
  inode.i_mode_ = EXT2_S_IFREG | (mode & 0777);
  inode.i_uid_ = getuid();
  inode.i_gid_ = getgid();
  inode.i_links_count_ = 1;
  inode.i_atime_ = time(nullptr);
  inode.i_ctime_ = time(nullptr);
  inode.i_mtime_ = time(nullptr);
  return my_fs->create(path, inode);
}

static int my_unlink(const char *path) {
  std::lock_guard<std::mutex> guard(my_mutex);
  return my_fs->unlink(path);
}

static int my_rename(const char *oldpath, const char *newpath,
                     unsigned int flags) {
  std::lock_guard<std::mutex> guard(my_mutex);
  return my_fs->rename(oldpath, newpath);
}

static int my_rmdir(const char *path) {
  std::lock_guard<std::mutex> guard(my_mutex);
  return my_fs->rmdir(path);
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
  stbuf->st_uid = inode.i_uid_;
  stbuf->st_gid = inode.i_gid_;
  stbuf->st_atime = inode.i_atime_;
  stbuf->st_ctime = inode.i_ctime_;
  stbuf->st_mtime = inode.i_mtime_;
  stbuf->st_nlink = inode.i_links_count_;
  stbuf->st_blksize = BLOCK_SIZE;
  stbuf->st_blocks = inode.i_blocks_;
  return 0;
}

static int my_chmod(const char *path, mode_t mode, struct fuse_file_info *fi) {
  uint32_t iid;
  inode inode;
  if (!my_fs->readdir(path, &inode, &iid)) return -ENOENT;

  inode.i_mode_ = (inode.i_mode_ & ~07777) | (mode & 07777);
  my_fs->im_->write_inode(inode, iid);
  return 0;
}
static int my_chown(const char *path, uid_t uid, gid_t gid,
                    struct fuse_file_info *fi) {
  uint32_t iid;
  inode inode;
  if (!my_fs->readdir(path, &inode, &iid)) return -ENOENT;

  inode.i_uid_ = uid;
  inode.i_gid_ = gid;
  my_fs->im_->write_inode(inode, iid);
  return 0;
}

static struct fuse_operations my_oper = {
    .getattr = hello_getattr,
    .mkdir = my_mkdir,
    .unlink = my_unlink,
    .rmdir = my_rmdir,
    .rename = my_rename,
    .chmod = my_chmod,
    .chown = my_chown,
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
