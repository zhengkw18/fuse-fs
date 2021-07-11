#ifndef __EFS_H_
#define __EFS_H_

#include "vfs.h"
#include "bitmap.h"

class EasyFileSystem
{
    shared_ptr<BlockDevice> block_device;
    shared_ptr<Bitmap> inode_bitmap;
    shared_ptr<Bitmap> data_bitmap;
    shared_ptr<Inode> root;
    u32 inode_area_start_block;
    u32 data_area_start_block;
    u32 uid, gid;

public:
    EasyFileSystem(shared_ptr<BlockDevice> _block_device, shared_ptr<Bitmap> _inode_bitmap, shared_ptr<Bitmap> _data_bitmap, u32 _inode_area_start_block, u32 _data_area_start_block);
    static shared_ptr<EasyFileSystem> open(shared_ptr<BlockDevice> _block_device);
    static shared_ptr<EasyFileSystem> create(shared_ptr<BlockDevice> _block_device);
    void get_disk_inode_pos(u32 inode_id, u32 &block_id, u32 &block_offset);
    u32 get_inode_id(u32 block_id, u32 block_offset);
    u32 alloc_inode();
    u32 alloc_data();
    void dealloc_inode(u32 inode_id);
    void dealloc_data(u32 block_id);
    shared_ptr<Inode> find(string path, i32 &err);
    shared_ptr<Inode> create(string path, DiskInodeType type, i32 &err, u32 mode);
    i64 unlink(string path);
    i64 rename(string from, string to);
    i64 link(string from, string to);
    shared_ptr<Inode> get_inode(u32 inode_id);
    void set_user(u32 _uid, u32 _gid);
    void get_user(u32 &_uid, u32 &_gid);
};
#endif