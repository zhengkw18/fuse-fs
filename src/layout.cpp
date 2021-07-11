#include "layout.h"

void SuperBlock::initialize(u32 _total_blocks, u32 _inode_bitmap_blocks, u32 _inode_area_blocks, u32 _data_bitmap_blocks, u32 _data_area_blocks)
{
    magic = efs_magic;
    total_blocks = _total_blocks;
    inode_bitmap_blocks = _inode_bitmap_blocks;
    inode_area_blocks = _inode_area_blocks;
    data_bitmap_blocks = _data_bitmap_blocks;
    data_area_blocks = _data_area_blocks;
}
bool SuperBlock::is_valid() const
{
    return magic == efs_magic;
}

void DiskInode::initialize(DiskInodeType _type)
{
    size = 0;
    memset(direct, 0, inode_direct_count * sizeof(u32));
    indirect1 = 0;
    indirect2 = 0;
    nlink = 1;
    type = _type;
    dirent_num = 0;
    uid = gid = 0;
    mode = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH;
    timespec time_s;
    clock_gettime(CLOCK_REALTIME, &time_s);
    atime = ctime = time_s.tv_sec;
}

u32 DiskInode::get_dirent_num() const
{
    return dirent_num;
}
void DiskInode::add_dirent_num()
{
    dirent_num++;
}
void DiskInode::sub_dirent_num()
{
    dirent_num--;
}

i64 DiskInode::get_atime() const
{
    return atime;
}
void DiskInode::refresh_atime()
{
    timespec time_s;
    clock_gettime(CLOCK_REALTIME, &time_s);
    atime = time_s.tv_sec;
}
i64 DiskInode::get_ctime() const
{
    return ctime;
}
void DiskInode::refresh_ctime()
{
    timespec time_s;
    clock_gettime(CLOCK_REALTIME, &time_s);
    ctime = time_s.tv_sec;
}

void DiskInode::set_atime(i64 time)
{
    atime = time;
}
void DiskInode::set_ctime(i64 time)
{
    ctime = time;
}
u32 DiskInode::get_uid() const
{
    return uid;
}
void DiskInode::set_uid(u32 _uid)
{
    uid = _uid;
}
u32 DiskInode::get_gid() const
{
    return gid;
}
void DiskInode::set_gid(u32 _gid)
{
    gid = _gid;
}
u32 DiskInode::get_mode() const
{
    return mode;
}
void DiskInode::set_mode(u32 _mode)
{
    mode = _mode;
}

u32 DiskInode::get_size() const
{
    return size;
}

u32 DiskInode::get_nlink() const
{
    return nlink;
}
void DiskInode::add_nlink()
{
    nlink++;
}
void DiskInode::sub_nlink()
{
    nlink--;
}

bool DiskInode::is_dir() const
{
    return type == DiskInodeType::Directory;
}
bool DiskInode::is_file() const
{
    return type == DiskInodeType::File;
}
u32 DiskInode::data_blocks() const
{
    return _data_blocks(size);
}
u32 DiskInode::_data_blocks(u32 _size) const
{
    return (_size + block_sz - 1) / block_sz;
}
u32 DiskInode::total_blocks(u32 _size) const
{
    u32 n_data_blocks = _data_blocks(_size);
    u32 total = n_data_blocks;
    if (n_data_blocks > inode_direct_count)
    {
        total += 1;
    }
    if (n_data_blocks > indirect1_bound)
    {
        total += 1;
        total += (n_data_blocks - indirect1_bound + inode_indirect1_count - 1) / inode_indirect1_count;
    }
    return total;
}
u32 DiskInode::blocks_num_needed(u32 new_size)
{
    assert(new_size > size);
    return total_blocks(new_size) - total_blocks(size);
}
u32 DiskInode::get_block_id(u32 inner_id, shared_ptr<BlockDevice> device) const
{
    if (inner_id < inode_direct_count)
    {
        return direct[inner_id];
    }
    else if (inner_id < indirect1_bound)
    {
        return BLOCK_CACHE_MANAGER
            .get_block_cache(indirect1, device, -1)
            .get()
            ->read<IndirectBlock, u32>(0, [inner_id](const IndirectBlock &indirect_block) -> u32
                                       { return indirect_block.data[inner_id - inode_direct_count]; });
    }
    else if (inner_id < indirect2_bound)
    {
        u32 last = inner_id - indirect1_bound;
        u32 _indirect1 = BLOCK_CACHE_MANAGER
                             .get_block_cache(indirect2, device, -1)
                             .get()
                             ->read<IndirectBlock, u32>(0, [last](const IndirectBlock &indirect_block) -> u32
                                                        { return indirect_block.data[last / inode_indirect1_count]; });
        return BLOCK_CACHE_MANAGER
            .get_block_cache(_indirect1, device, -1)
            .get()
            ->read<IndirectBlock, u32>(0, [last](const IndirectBlock &indirect_block) -> u32
                                       { return indirect_block.data[last % inode_indirect1_count]; });
    }
    return 0;
}
bool DiskInode::increase_size(u32 new_size, vector<u32> new_blocks, shared_ptr<BlockDevice> device)
{
    assert(new_size <= indirect2_bound * block_sz);
    u32 current_blocks = data_blocks();
    size = new_size;
    u32 total_blocks = data_blocks();
    auto iter = new_blocks.begin();
    while (current_blocks < min(total_blocks, inode_direct_count))
    {
        direct[current_blocks] = *iter;
        iter++;
        current_blocks++;
    }
    if (total_blocks > inode_direct_count)
    {
        if (current_blocks == inode_direct_count)
        {
            indirect1 = *iter;
            iter++;
        }
        current_blocks -= inode_direct_count;
        total_blocks -= inode_direct_count;
    }
    else
    {
        return true;
    }
    BLOCK_CACHE_MANAGER
        .get_block_cache(indirect1, device, -1)
        .get()
        ->modify_and_sync<IndirectBlock, u32>(0, [&current_blocks, total_blocks, &iter](IndirectBlock &indirect_block) -> u32
                                              {
                                                  while (current_blocks < min(total_blocks, inode_indirect1_count))
                                                  {
                                                      indirect_block.data[current_blocks] = *iter;
                                                      iter++;
                                                      current_blocks++;
                                                  }
                                                  return 0;
                                              });
    if (total_blocks > inode_indirect1_count)
    {
        if (current_blocks == inode_indirect1_count)
        {
            indirect2 = *iter;
            iter++;
        }
        current_blocks -= inode_indirect1_count;
        total_blocks -= inode_indirect1_count;
    }
    else
    {
        return true;
    }
    u32 a0 = current_blocks / inode_indirect1_count;
    u32 b0 = current_blocks % inode_indirect1_count;
    u32 a1 = total_blocks / inode_indirect1_count;
    u32 b1 = total_blocks % inode_indirect1_count;
    BLOCK_CACHE_MANAGER
        .get_block_cache(indirect2, device, -1)
        .get()
        ->modify_and_sync<IndirectBlock, u32>(0, [&a0, &b0, a1, b1, total_blocks, &iter, device](IndirectBlock &indirect2_block) -> u32
                                              {
                                                  while ((a0 < a1) || (a0 == a1 && b0 < b1))
                                                  {
                                                      if (b0 == 0)
                                                      {
                                                          indirect2_block.data[a0] = *iter;
                                                          iter++;
                                                      }
                                                      BLOCK_CACHE_MANAGER
                                                          .get_block_cache(indirect2_block.data[a0], device, -1)
                                                          .get()
                                                          ->modify_and_sync<IndirectBlock, u32>(0, [b0, &iter](IndirectBlock &indirect1_block) -> u32
                                                                                                {
                                                                                                    indirect1_block.data[b0] = *iter;
                                                                                                    iter++;
                                                                                                    return 0;
                                                                                                });
                                                      b0++;
                                                      if (b0 == inode_indirect1_count)
                                                      {
                                                          b0 = 0;
                                                          a0++;
                                                      }
                                                  }
                                                  return 0;
                                              });
    return true;
}
vector<u32> DiskInode::clear_size(shared_ptr<BlockDevice> device)
{
    vector<u32> v;
    u32 n_data_blocks = data_blocks();
    size = 0;
    u32 current_blocks = 0;
    while (current_blocks < min(n_data_blocks, inode_direct_count))
    {
        v.push_back(direct[current_blocks]);
        direct[current_blocks] = 0;
        current_blocks++;
    }
    if (n_data_blocks > inode_direct_count)
    {
        v.push_back(indirect1);
        n_data_blocks -= inode_direct_count;
        current_blocks = 0;
    }
    else
    {
        return v;
    }
    BLOCK_CACHE_MANAGER
        .get_block_cache(indirect1, device, -1)
        .get()
        ->read<IndirectBlock, u32>(0, [&current_blocks, n_data_blocks, &v](const IndirectBlock &indirect_block) -> u32
                                   {
                                       while (current_blocks < min(n_data_blocks, inode_indirect1_count))
                                       {
                                           v.push_back(indirect_block.data[current_blocks]);
                                           current_blocks++;
                                       }
                                       return 0;
                                   });
    indirect1 = 0;
    if (n_data_blocks > inode_indirect1_count)
    {
        v.push_back(indirect2);
        n_data_blocks -= inode_indirect1_count;
    }
    else
    {
        return v;
    }
    u32 a1 = n_data_blocks / inode_indirect1_count;
    u32 b1 = n_data_blocks % inode_indirect1_count;
    BLOCK_CACHE_MANAGER
        .get_block_cache(indirect2, device, -1)
        .get()
        ->read<IndirectBlock, u32>(0, [a1, b1, n_data_blocks, &v, device](const IndirectBlock &indirect2_block) -> u32
                                   {
                                       u32 a0 = 0;
                                       u32 b0 = 0;
                                       while ((a0 < a1) || (a0 == a1 && b0 < b1))
                                       {
                                           if (b0 == 0)
                                           {
                                               v.push_back(indirect2_block.data[a0]);
                                           }
                                           BLOCK_CACHE_MANAGER
                                               .get_block_cache(indirect2_block.data[a0], device, -1)
                                               .get()
                                               ->read<IndirectBlock, u32>(0, [b0, &v](const IndirectBlock &indirect1_block) -> u32
                                                                          {
                                                                              v.push_back(indirect1_block.data[b0]);
                                                                              return 0;
                                                                          });
                                           b0++;
                                           if (b0 == inode_indirect1_count)
                                           {
                                               b0 = 0;
                                               a0++;
                                           }
                                       }
                                       return 0;
                                   });
    indirect2 = 0;
    return v;
}
u32 DiskInode::read_at(u32 offset, u8 *buf, u32 _size, shared_ptr<BlockDevice> device, i32 inode_id)
{
    u32 start = offset;
    u32 end = min(offset + _size, size);
    if (start >= end)
    {
        return 0;
    }
    u32 start_block = start / block_sz;
    u32 read_size = 0;
    while (1)
    {
        u32 end_current_block = min((start / block_sz + 1) * block_sz, end);
        u32 block_read_size = end_current_block - start;
        u8 *dst = buf + read_size;
        BLOCK_CACHE_MANAGER
            .get_block_cache(get_block_id(start_block, device), device, inode_id)
            .get()
            ->read<Block, u32>(0, [dst, start, block_read_size](const Block &data_block) -> u32
                               {
                                   memcpy(dst, data_block.data + start % block_sz, block_read_size);
                                   return 0;
                               });
        read_size += block_read_size;
        if (end_current_block == end)
        {
            break;
        }
        start_block += 1;
        start = end_current_block;
    }
    refresh_atime();
    return read_size;
}
u32 DiskInode::write_at(u32 offset, const u8 *buf, u32 _size, shared_ptr<BlockDevice> device, i32 inode_id)
{
    u32 start = offset;
    u32 end = min(offset + _size, size);
    assert(start <= end);
    u32 start_block = start / block_sz;
    u32 write_size = 0;
    while (1)
    {
        u32 end_current_block = min((start / block_sz + 1) * block_sz, end);
        u32 block_write_size = end_current_block - start;
        const u8 *src = buf + write_size;
        if (inode_id == -1)
            BLOCK_CACHE_MANAGER
                .get_block_cache(get_block_id(start_block, device), device, inode_id)
                .get()
                ->modify_and_sync<Block, u32>(0, [src, start, block_write_size](Block &data_block) -> u32
                                              {
                                                  memcpy(data_block.data + start % block_sz, src, block_write_size);
                                                  return 0;
                                              });
        else
            BLOCK_CACHE_MANAGER
                .get_block_cache(get_block_id(start_block, device), device, inode_id)
                .get()
                ->modify<Block, u32>(0, [src, start, block_write_size](Block &data_block) -> u32
                                     {
                                         memcpy(data_block.data + start % block_sz, src, block_write_size);
                                         return 0;
                                     });
        write_size += block_write_size;
        if (end_current_block == end)
        {
            break;
        }
        start_block += 1;
        start = end_current_block;
    }
    refresh_ctime();
    return write_size;
}

bool DiskInode::permit_r(u32 _uid, u32 _gid) const
{
    return have_r_permission(mode, uid, gid, _uid, _gid);
}
bool DiskInode::permit_w(u32 _uid, u32 _gid) const
{
    return have_w_permission(mode, uid, gid, _uid, _gid);
}
bool DiskInode::permit_x(u32 _uid, u32 _gid) const
{
    return have_x_permission(mode, uid, gid, _uid, _gid);
}

DirEntry::DirEntry()
{
    memset(name, 0, name_length_limit + 1);
    inode_number = 0;
}
DirEntry::DirEntry(string _name, u32 _inode_number)
{
    u32 length = _name.length();
    assert(length <= name_length_limit);
    memcpy(name, _name.c_str(), length);
    name[length] = 0;
    inode_number = _inode_number;
}
string DirEntry::get_name()
{
    return string((char *)name);
}
u32 DirEntry::get_inode_number()
{
    return inode_number;
}
const u8 *DirEntry::as_bytes()
{
    return name;
}
u8 *DirEntry::as_bytes_mut()
{
    return name;
}