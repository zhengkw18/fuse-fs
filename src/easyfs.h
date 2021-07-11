#ifndef __EASYFS_H_
#define __EASYFS_H_

#include "Fuse.h"

#include "Fuse-impl.h"
#include "efs.h"

class EasyFS : public Fusepp::Fuse<EasyFS>
{
  shared_ptr<BlockDevice> device;
  shared_ptr<EasyFileSystem> fs;
  u32 uid, gid;
  pthread_mutex_t inode_lock;

public:
  EasyFS(shared_ptr<BlockDevice> _device, shared_ptr<EasyFileSystem> _fs, u32 _uid, u32 _gid)
  {
    device = _device;
    fs = _fs;
    uid = _uid;
    gid = _gid;
    pthread_mutex_init(&inode_lock, nullptr);
  }
  ~EasyFS()
  {
    pthread_mutex_destroy(&inode_lock);
  }

  static int getattr(const char *path, struct stat *stbuf, struct fuse_file_info *);

  static int opendir(const char *path, struct fuse_file_info *fi);

  static int readdir(const char *, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);

  static int releasedir(const char *, struct fuse_file_info *fi);

  static int open(const char *path, struct fuse_file_info *fi);

  static int read(const char *, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

  static int write(const char *, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

  static int fsync(const char *, int isdatasync, struct fuse_file_info *fi);

  static int release(const char *, struct fuse_file_info *fi);

  static int create(const char *path, mode_t mode, struct fuse_file_info *fi);

  static int mkdir(const char *path, mode_t mode);

  static int unlink(const char *path);

  static int rmdir(const char *path);

  static int rename(const char *from, const char *to, unsigned int flags);

  static int link(const char *from, const char *to);

  static int chmod(const char *path, mode_t mode, struct fuse_file_info *);

  static int chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi);

  static int utimens(const char *path, const struct timespec ts[2], struct fuse_file_info *fi);
};

#endif
