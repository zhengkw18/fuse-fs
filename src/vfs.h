#ifndef __VFS_H_
#define __VFS_H_

#include "layout.h"
class EasyFileSystem;
class Inode
{
    u32 inode_id;
    u32 block_id;
    u32 block_offset;
    EasyFileSystem *fs;
    shared_ptr<BlockDevice> block_device;

public:
    Inode(u32 _inode_id, u32 _block_id, u32 _block_offset, EasyFileSystem *_fs, shared_ptr<BlockDevice> _block_device);
    u32 get_id();

    template <typename V>
    V read_disk_inode(function<V(const DiskInode &)> f)
    {
        return BLOCK_CACHE_MANAGER.get_block_cache(block_id, block_device, -1).get()->read<DiskInode, V>(block_offset, f);
    }

    template <typename V>
    V modify_disk_inode(function<V(DiskInode &)> f)
    {
        return BLOCK_CACHE_MANAGER.get_block_cache(block_id, block_device, -1).get()->modify_and_sync<DiskInode, V>(block_offset, f);
    }

    i64 find_inode_id(string name, DiskInode &disk_inode);

    shared_ptr<Inode> find(string name);

    void increase_size(u32 new_size, DiskInode &disk_inode);

    shared_ptr<Inode> create(string name, DiskInodeType type, u32 uid, u32 gid, u32 mode);

    void remove(string name);

    vector<pair<string, u32>> ls();

    u32 read_at(u32 offset, u8 *buf, u32 _size);

    u32 write_at(u32 offset, const u8 *buf, u32 _size);

    void clear();

    bool is_dir();

    bool is_file();

    void sync();

    u32 get_nlink();

    void add_nlink();

    bool sub_nlink();

    void rename(string old_name, string new_name);

    void link(string name, u32 inode_id);

    struct stat get_stat();

    u32 get_dirent_num();

    void set_mode(u32 mode);

    void set_owner(u32 uid, u32 gid);

    void get_permission(u32 &mode, u32 &uid, u32 &gid);

    bool permit_r(u32 _uid, u32 _gid);
    bool permit_w(u32 _uid, u32 _gid);
    bool permit_x(u32 _uid, u32 _gid);
    void set_atime(i64 time);
    void set_ctime(i64 time);
};

#endif