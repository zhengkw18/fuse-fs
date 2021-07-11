#include "easyfs.h"

#include <iostream>
#include <string>

#include "Fuse-impl.h"

using namespace std;

int EasyFS::getattr(const char *path, struct stat *stbuf, struct fuse_file_info *)
{
	pthread_mutex_lock(&this_()->inode_lock);
	i32 err;
	shared_ptr<EasyFileSystem> fs = this_()->fs;
	shared_ptr<Inode> inode = fs.get()->find(path, err);
	if (inode == nullptr)
	{
		pthread_mutex_unlock(&this_()->inode_lock);
		return -err;
	}
	u32 uid, gid;
	fs.get()->get_user(uid, gid);
	if (!inode.get()->permit_r(uid, gid))
	{
		pthread_mutex_unlock(&this_()->inode_lock);
		return -EACCES;
	}
	*stbuf = inode.get()->get_stat();
	pthread_mutex_unlock(&this_()->inode_lock);
	return 0;
}

int EasyFS::opendir(const char *path, struct fuse_file_info *fi)
{
	pthread_mutex_lock(&this_()->inode_lock);
	i32 err;
	shared_ptr<EasyFileSystem> fs = this_()->fs;
	shared_ptr<Inode> inode = fs.get()->find(path, err);
	if (inode == nullptr)
	{
		pthread_mutex_unlock(&this_()->inode_lock);
		return -err;
	}
	if (!inode.get()->is_dir())
	{
		pthread_mutex_unlock(&this_()->inode_lock);
		return -ENOTDIR;
	}
	u32 uid, gid;
	fs.get()->get_user(uid, gid);
	u32 flags = fi->flags & O_ACCMODE;
	if ((flags == O_RDONLY || flags == O_RDWR) && !inode.get()->permit_r(uid, gid))
	{
		pthread_mutex_unlock(&this_()->inode_lock);
		return -EACCES;
	}
	if ((flags == O_WRONLY || flags == O_RDWR) && !inode.get()->permit_w(uid, gid))
	{
		pthread_mutex_unlock(&this_()->inode_lock);
		return -EACCES;
	}
	fi->fh = inode.get()->get_id();
	pthread_mutex_unlock(&this_()->inode_lock);
	return 0;
}

int EasyFS::readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
	if (fi->fh == (uint64_t)-1)
		return -ENOENT;
	shared_ptr<EasyFileSystem> fs = this_()->fs;
	shared_ptr<Inode> inode = fs.get()->get_inode(fi->fh);
	auto childs = inode.get()->ls();
	for (auto child : childs)
	{
		shared_ptr<Inode> inode_child = fs.get()->get_inode(child.second);
		struct stat st = inode_child.get()->get_stat();
		filler(buf, child.first.c_str(), &st, 0, FUSE_FILL_DIR_PLUS);
	}
	return 0;
}

int EasyFS::releasedir(const char *, struct fuse_file_info *fi)
{
	fi->fh = (uint64_t)-1;
	return 0;
}

int EasyFS::open(const char *path, struct fuse_file_info *fi)
{
	pthread_mutex_lock(&this_()->inode_lock);
	i32 err;
	shared_ptr<EasyFileSystem> fs = this_()->fs;
	shared_ptr<Inode> inode = fs.get()->find(path, err);
	if (inode == nullptr)
	{
		pthread_mutex_unlock(&this_()->inode_lock);
		return -err;
	}
	if (inode.get()->is_dir())
	{
		pthread_mutex_unlock(&this_()->inode_lock);
		return -EISDIR;
	}
	u32 uid, gid;
	fs.get()->get_user(uid, gid);
	u32 flags = fi->flags & O_ACCMODE;
	if ((flags == O_RDONLY || flags == O_RDWR) && !inode.get()->permit_r(uid, gid))
	{
		pthread_mutex_unlock(&this_()->inode_lock);
		return -EACCES;
	}
	if ((flags == O_WRONLY || flags == O_RDWR) && !inode.get()->permit_w(uid, gid))
	{
		pthread_mutex_unlock(&this_()->inode_lock);
		return -EACCES;
	}
	if (fi->flags & O_TRUNC)
		inode.get()->clear();
	fi->fh = inode.get()->get_id();
	pthread_mutex_unlock(&this_()->inode_lock);
	return 0;
}

int EasyFS::read(const char *, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	if (fi->fh == (uint64_t)-1)
		return -ENOENT;
	shared_ptr<EasyFileSystem> fs = this_()->fs;
	shared_ptr<Inode> inode = fs.get()->get_inode(fi->fh);
	return inode->read_at(offset, (u8 *)buf, size);
}

int EasyFS::write(const char *, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	if (fi->fh == (uint64_t)-1)
		return -ENOENT;
	shared_ptr<EasyFileSystem> fs = this_()->fs;
	shared_ptr<Inode> inode = fs.get()->get_inode(fi->fh);
	return inode->write_at(offset, (u8 *)buf, size);
}

int EasyFS::fsync(const char *, int isdatasync, struct fuse_file_info *fi)
{
	if (fi->fh == (uint64_t)-1)
		return -ENOENT;
	shared_ptr<EasyFileSystem> fs = this_()->fs;
	shared_ptr<Inode> inode = fs.get()->get_inode(fi->fh);
	inode->sync();
	return 0;
}

int EasyFS::release(const char *, struct fuse_file_info *fi)
{
	if (fi->fh == (uint64_t)-1)
		return -ENOENT;
	shared_ptr<EasyFileSystem> fs = this_()->fs;
	shared_ptr<Inode> inode = fs.get()->get_inode(fi->fh);
	inode->sync();
	fi->fh = (uint64_t)-1;
	return 0;
}

int EasyFS::create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	pthread_mutex_lock(&this_()->inode_lock);
	i32 err;
	shared_ptr<EasyFileSystem> fs = this_()->fs;
	shared_ptr<Inode> inode = fs.get()->create(path, DiskInodeType::File, err, mode);
	if (inode == nullptr)
	{
		pthread_mutex_unlock(&this_()->inode_lock);
		return -err;
	}
	fi->fh = inode.get()->get_id();
	pthread_mutex_unlock(&this_()->inode_lock);
	return 0;
}

int EasyFS::mkdir(const char *path, mode_t mode)
{
	pthread_mutex_lock(&this_()->inode_lock);
	i32 err;
	shared_ptr<EasyFileSystem> fs = this_()->fs;
	shared_ptr<Inode> inode = fs.get()->create(path, DiskInodeType::Directory, err, mode);
	if (inode == nullptr)
	{
		pthread_mutex_unlock(&this_()->inode_lock);
		return -err;
	}
	pthread_mutex_unlock(&this_()->inode_lock);
	return 0;
}

int EasyFS::unlink(const char *path)
{
	pthread_mutex_lock(&this_()->inode_lock);
	shared_ptr<EasyFileSystem> fs = this_()->fs;
	i64 re = fs.get()->unlink(path);
	pthread_mutex_unlock(&this_()->inode_lock);
	return re;
}

int EasyFS::rmdir(const char *path)
{
	pthread_mutex_lock(&this_()->inode_lock);
	shared_ptr<EasyFileSystem> fs = this_()->fs;
	i64 re = fs.get()->unlink(path);
	pthread_mutex_unlock(&this_()->inode_lock);
	return re;
}

int EasyFS::rename(const char *from, const char *to, unsigned int flags)
{
	pthread_mutex_lock(&this_()->inode_lock);
	shared_ptr<EasyFileSystem> fs = this_()->fs;
	i64 re = fs.get()->rename(from, to);
	pthread_mutex_unlock(&this_()->inode_lock);
	return re;
}

int EasyFS::link(const char *from, const char *to)
{
	pthread_mutex_lock(&this_()->inode_lock);
	shared_ptr<EasyFileSystem> fs = this_()->fs;
	i64 re = fs.get()->link(from, to);
	pthread_mutex_unlock(&this_()->inode_lock);
	return re;
}

int EasyFS::chmod(const char *path, mode_t mode, struct fuse_file_info *)
{
	pthread_mutex_lock(&this_()->inode_lock);
	i32 err;
	shared_ptr<EasyFileSystem> fs = this_()->fs;
	shared_ptr<Inode> inode = fs.get()->find(path, err);
	if (inode == nullptr)
	{
		pthread_mutex_unlock(&this_()->inode_lock);
		return -err;
	}
	inode.get()->set_mode(mode);
	pthread_mutex_unlock(&this_()->inode_lock);
	return 0;
}

int EasyFS::chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *)
{
	pthread_mutex_lock(&this_()->inode_lock);
	i32 err;
	shared_ptr<EasyFileSystem> fs = this_()->fs;
	shared_ptr<Inode> inode = fs.get()->find(path, err);
	if (inode == nullptr)
	{
		pthread_mutex_unlock(&this_()->inode_lock);
		return -err;
	}
	inode.get()->set_owner(uid, gid);
	pthread_mutex_unlock(&this_()->inode_lock);
	return 0;
}

int EasyFS::utimens(const char *path, const struct timespec ts[2], struct fuse_file_info *fi)
{
	return 0;
}