#ifndef __LAYOUT_H_
#define __LAYOUT_H_

#include "block_cache.h"

struct SuperBlock
{
    u32 magic;
    u32 total_blocks;
    u32 inode_bitmap_blocks;
    u32 inode_area_blocks;
    u32 data_bitmap_blocks;
    u32 data_area_blocks;

public:
    void initialize(u32 _total_blocks, u32 _inode_bitmap_blocks, u32 _inode_area_blocks, u32 _data_bitmap_blocks, u32 _data_area_blocks);
    bool is_valid() const;
};

struct IndirectBlock
{
    u32 data[block_sz / 4];
};

enum DiskInodeType : u32
{
    File,
    Directory
};

class DiskInode
{
    u32 size;
    u32 direct[inode_direct_count];
    u32 indirect1;
    u32 indirect2;
    u32 nlink;
    u32 uid;
    u32 gid;
    u32 mode;
    DiskInodeType type;
    u32 dirent_num;
    i64 atime;
    i64 ctime;

public:
    u32 get_dirent_num() const;
    void add_dirent_num();
    void sub_dirent_num();
    i64 get_atime() const;
    void set_atime(i64 time);
    void refresh_atime();
    i64 get_ctime() const;
    void set_ctime(i64 time);
    void refresh_ctime();
    u32 get_uid() const;
    void set_uid(u32 _uid);
    u32 get_gid() const;
    void set_gid(u32 _gid);
    u32 get_mode() const;
    void set_mode(u32 _mode);
    u32 get_nlink() const;
    void add_nlink();
    void sub_nlink();
    u32 get_size() const;
    void initialize(DiskInodeType _type);
    bool is_dir() const;
    bool is_file() const;
    u32 data_blocks() const;
    u32 _data_blocks(u32 _size) const;
    u32 total_blocks(u32 _size) const;
    u32 blocks_num_needed(u32 new_size);
    u32 get_block_id(u32 inner_id, shared_ptr<BlockDevice> device) const;
    bool increase_size(u32 new_size, vector<u32> new_blocks, shared_ptr<BlockDevice> device);
    vector<u32> clear_size(shared_ptr<BlockDevice> device);
    u32 read_at(u32 offset, u8 *buf, u32 _size, shared_ptr<BlockDevice> device, i32 inode_id);
    u32 write_at(u32 offset, const u8 *buf, u32 _size, shared_ptr<BlockDevice> device, i32 inode_id);
    bool permit_r(u32 _uid, u32 _gid) const;
    bool permit_w(u32 _uid, u32 _gid) const;
    bool permit_x(u32 _uid, u32 _gid) const;
};

class DirEntry
{
    u8 name[name_length_limit + 1];
    u32 inode_number;

public:
    DirEntry();
    DirEntry(string _name, u32 _inode_number);
    string get_name();
    u32 get_inode_number();
    const u8 *as_bytes();
    u8 *as_bytes_mut();
};

#endif