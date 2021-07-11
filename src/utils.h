#ifndef __UTILS_H_
#define __UTILS_H_

#include <string>
#include <memory>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>
#include <assert.h>
#include <string.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <deque>

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef char i8;
typedef int i32;
typedef long long i64;

using namespace std;

const string root_file = "/tmp/disk";

const u32 device_num = 4;

const u32 block_sz = 512;

const u32 device_sz = 1 * 1024 * 1024 * 1024; //1G

const u32 device_block_num = device_sz / block_sz;

const u32 block_cache_group = 4096;

const u32 block_cache_way = 16;

const u32 block_bits = block_sz * 8;

const u32 num_u64_per_block = block_bits / 64;

const u32 efs_magic = 0x3b800001;

const u32 inode_direct_count = 19;

const u32 name_length_limit = 27;
const u32 inode_indirect1_count = block_sz / 4;
const u32 inode_indirect2_count = inode_indirect1_count * inode_indirect1_count;
const u32 direct_bound = inode_direct_count;
const u32 indirect1_bound = direct_bound + inode_indirect1_count;
const u32 indirect2_bound = indirect1_bound + inode_indirect2_count;

const u32 dirent_sz = 32;
const u32 inode_size = 128;
const u32 inodes_per_block = block_sz / inode_size;

const u32 inode_bitmap_blocks = 16;

inline vector<string> split_path(string path)
{
    vector<string> paths;
    u32 last = 0;
    for (u32 i = 0; i < path.length(); i++)
    {
        if (path[i] == '/')
        {
            if (i > last)
                paths.push_back(path.substr(last, i - last));
            last = i + 1;
        }
    }
    if (last < path.length())
        paths.push_back(path.substr(last, path.length() - last));
    return paths;
}

inline bool have_r_permission(u32 mode, u32 owner_uid, u32 owner_gid, u32 uid, u32 gid)
{
    return uid == 0 || ((uid == owner_uid) && (mode & S_IRUSR)) || ((gid == owner_gid) && (mode & S_IRGRP)) || ((gid != owner_gid) && (mode & S_IROTH));
}

inline bool have_w_permission(u32 mode, u32 owner_uid, u32 owner_gid, u32 uid, u32 gid)
{
    return uid == 0 || ((uid == owner_uid) && (mode & S_IWUSR)) || ((gid == owner_gid) && (mode & S_IWGRP)) || ((gid != owner_gid) && (mode & S_IWOTH));
}

inline bool have_x_permission(u32 mode, u32 owner_uid, u32 owner_gid, u32 uid, u32 gid)
{
    return uid == 0 || ((uid == owner_uid) && (mode & S_IXUSR)) || ((gid == owner_gid) && (mode & S_IXGRP)) || ((gid != owner_gid) && (mode & S_IXOTH));
}

#endif