#include "efs.h"

EasyFileSystem::EasyFileSystem(shared_ptr<BlockDevice> _block_device, shared_ptr<Bitmap> _inode_bitmap, shared_ptr<Bitmap> _data_bitmap, u32 _inode_area_start_block, u32 _data_area_start_block)
{
    block_device = _block_device;
    inode_bitmap = _inode_bitmap;
    data_bitmap = _data_bitmap;
    inode_area_start_block = _inode_area_start_block;
    data_area_start_block = _data_area_start_block;
    u32 root_inode_block_id, root_inode_offset;
    get_disk_inode_pos(0, root_inode_block_id, root_inode_offset);
    root = shared_ptr<Inode>(new Inode(0, root_inode_block_id, root_inode_offset, this, _block_device));
    uid = gid = 0;
}
shared_ptr<EasyFileSystem> EasyFileSystem::open(shared_ptr<BlockDevice> _block_device)
{
    u32 inode_area_blocks, data_bitmap_blocks, data_area_blocks;
    bool valid = BLOCK_CACHE_MANAGER
                     .get_block_cache(0, _block_device, -1)
                     .get()
                     ->read<SuperBlock, bool>(0, [&inode_area_blocks, &data_bitmap_blocks, &data_area_blocks](const SuperBlock &super_block) -> u32
                                              {
                                                  if (!super_block.is_valid())
                                                      return false;
                                                  inode_area_blocks = super_block.inode_area_blocks;
                                                  data_bitmap_blocks = super_block.data_bitmap_blocks;
                                                  data_area_blocks = super_block.data_area_blocks;
                                                  return true;
                                              });
    if (!valid)
        return shared_ptr<EasyFileSystem>(nullptr);
    u32 inode_total_blocks = inode_bitmap_blocks + inode_area_blocks;
    shared_ptr<Bitmap> inode_bitmap = shared_ptr<Bitmap>(new Bitmap(1, inode_bitmap_blocks));
    shared_ptr<Bitmap> data_bitmap = shared_ptr<Bitmap>(new Bitmap(1 + inode_total_blocks, data_bitmap_blocks));
    shared_ptr<EasyFileSystem> efs = shared_ptr<EasyFileSystem>(new EasyFileSystem(_block_device, inode_bitmap, data_bitmap, 1 + inode_bitmap_blocks, 1 + inode_total_blocks + data_bitmap_blocks));
    assert(efs.get()->root->is_dir());
    return efs;
}
shared_ptr<EasyFileSystem> EasyFileSystem::create(shared_ptr<BlockDevice> _block_device)
{
    shared_ptr<Bitmap> inode_bitmap = shared_ptr<Bitmap>(new Bitmap(1, inode_bitmap_blocks));
    u32 inode_num = inode_bitmap.get()->maximum();
    u32 inode_area_blocks = (inode_num * inode_size + block_sz - 1) / block_sz;
    u32 inode_total_blocks = inode_bitmap_blocks + inode_area_blocks;
    u32 data_total_blocks = device_block_num - 1 - inode_total_blocks;
    u32 data_bitmap_blocks = (data_total_blocks + block_bits) / (block_bits + 1);
    u32 data_area_blocks = data_total_blocks - data_bitmap_blocks;
    shared_ptr<Bitmap> data_bitmap = shared_ptr<Bitmap>(new Bitmap(1 + inode_total_blocks, data_bitmap_blocks));
    inode_bitmap.get()->clear(_block_device);
    data_bitmap.get()->clear(_block_device);
    assert(inode_bitmap.get()->alloc(_block_device) == 0);
    shared_ptr<EasyFileSystem> efs = shared_ptr<EasyFileSystem>(new EasyFileSystem(_block_device, inode_bitmap, data_bitmap, 1 + inode_bitmap_blocks, 1 + inode_total_blocks + data_bitmap_blocks));
    BLOCK_CACHE_MANAGER
        .get_block_cache(0, _block_device, -1)
        .get()
        ->modify<SuperBlock, u32>(0, [inode_area_blocks, data_bitmap_blocks, data_area_blocks](SuperBlock &super_block) -> u32
                                  {
                                      super_block.initialize(device_block_num, inode_bitmap_blocks, inode_area_blocks, data_bitmap_blocks, data_area_blocks);
                                      return 0;
                                  });
    u32 root_inode_block_id, root_inode_offset;
    efs->get_disk_inode_pos(0, root_inode_block_id, root_inode_offset);
    BLOCK_CACHE_MANAGER
        .get_block_cache(root_inode_block_id, _block_device, -1)
        .get()
        ->modify<DiskInode, u32>(root_inode_offset, [](DiskInode &disk_inode) -> u32
                                 {
                                     disk_inode.initialize(DiskInodeType::Directory);
                                     return 0;
                                 });
    BLOCK_CACHE_MANAGER.flush();
    assert(efs->root.get()->is_dir());
    return efs;
}
void EasyFileSystem::get_disk_inode_pos(u32 inode_id, u32 &block_id, u32 &block_offset)
{
    block_id = inode_area_start_block + inode_id / inodes_per_block;
    block_offset = (inode_id % inodes_per_block) * inode_size;
}
u32 EasyFileSystem::get_inode_id(u32 block_id, u32 block_offset)
{
    return (block_id - inode_area_start_block) * inodes_per_block + block_offset / inode_size;
}
u32 EasyFileSystem::alloc_inode()
{
    return inode_bitmap.get()->alloc(block_device);
}
u32 EasyFileSystem::alloc_data()
{
    return data_bitmap.get()->alloc(block_device) + data_area_start_block;
}
void EasyFileSystem::dealloc_inode(u32 inode_id)
{
    inode_bitmap.get()->dealloc(block_device, inode_id);
}
void EasyFileSystem::dealloc_data(u32 block_id)
{
    data_bitmap.get()->dealloc(block_device, block_id - data_area_start_block);
}

shared_ptr<Inode> EasyFileSystem::find(string path, i32 &err)
{
    vector<string> paths = split_path(path);
    if (paths.size() == 0)
        return root;
    shared_ptr<Inode> parent = root;
    for (u32 i = 0; i < paths.size() - 1; i++)
    {
        if (!parent.get()->is_dir())
        {
            err = ENOTDIR;
            return shared_ptr<Inode>(nullptr);
        }
        if (!(parent.get()->permit_r(uid, gid) && parent.get()->permit_x(uid, gid)))
        {
            err = EACCES;
            return shared_ptr<Inode>(nullptr);
        }
        shared_ptr<Inode> child = parent.get()->find(paths[i]);
        if (child == nullptr)
        {
            err = ENOENT;
            return shared_ptr<Inode>(nullptr);
        }
        parent = child;
    }
    if (!parent.get()->is_dir())
    {
        err = ENOTDIR;
        return shared_ptr<Inode>(nullptr);
    }
    string last = paths.back();
    shared_ptr<Inode> child = parent.get()->find(last);
    if (child == nullptr)
        err = ENOENT;
    return child;
}
shared_ptr<Inode> EasyFileSystem::create(string path, DiskInodeType type, i32 &err, u32 mode)
{
    vector<string> paths = split_path(path);
    assert(paths.size() > 0);
    shared_ptr<Inode> parent = root;
    for (u32 i = 0; i < paths.size() - 1; i++)
    {
        if (!parent.get()->is_dir())
        {
            err = ENOTDIR;
            return shared_ptr<Inode>(nullptr);
        }
        if (!(parent.get()->permit_r(uid, gid) && parent.get()->permit_x(uid, gid)))
        {
            err = EACCES;
            return shared_ptr<Inode>(nullptr);
        }
        shared_ptr<Inode> child = parent.get()->find(paths[i]);
        if (child == nullptr)
        {
            err = ENOENT;
            return shared_ptr<Inode>(nullptr);
        }
        parent = child;
    }
    if (!parent.get()->is_dir())
    {
        err = ENOTDIR;
        return shared_ptr<Inode>(nullptr);
    }
    if (!parent.get()->permit_w(uid, gid))
    {
        err = EACCES;
        return shared_ptr<Inode>(nullptr);
    }
    string last = paths.back();
    if (last.size() > name_length_limit)
    {
        err = ENAMETOOLONG;
        return shared_ptr<Inode>(nullptr);
    }
    if (parent.get()->find(last) != nullptr)
    {
        err = EEXIST;
        return shared_ptr<Inode>(nullptr);
    }
    shared_ptr<Inode> child = parent.get()->create(last, type, uid, gid, mode);
    return child;
}
i64 EasyFileSystem::unlink(string path)
{
    vector<string> paths = split_path(path);
    assert(paths.size() > 0);
    shared_ptr<Inode> parent = root;
    for (u32 i = 0; i < paths.size() - 1; i++)
    {
        if (!parent.get()->is_dir())
            return -ENOTDIR;
        if (!(parent.get()->permit_r(uid, gid) && parent.get()->permit_x(uid, gid)))
            return -EACCES;
        shared_ptr<Inode> child = parent.get()->find(paths[i]);
        if (child == nullptr)
            return -ENOENT;
        parent = child;
    }
    if (!parent.get()->is_dir())
        return -ENOTDIR;
    if (!parent.get()->permit_w(uid, gid))
        return -EACCES;
    string last = paths.back();
    shared_ptr<Inode> child = parent.get()->find(last);
    if (child == nullptr)
        return -ENOENT;
    if (child.get()->is_dir() && child.get()->get_dirent_num() > 0)
    {
        return -ENOTEMPTY;
    }
    parent.get()->remove(last);
    if (child.get()->sub_nlink())
    {
        child.get()->clear();
        dealloc_inode(child.get()->get_id());
    }
    return 0;
}
i64 EasyFileSystem::rename(string from, string to)
{
    vector<string> paths_from = split_path(from);
    vector<string> paths_to = split_path(to);
    assert(paths_from.size() > 0 && paths_to.size() > 0);
    shared_ptr<Inode> parent_from = root;
    for (u32 i = 0; i < paths_from.size() - 1; i++)
    {
        if (!parent_from.get()->is_dir())
            return -ENOTDIR;
        if (!(parent_from.get()->permit_r(uid, gid) && parent_from.get()->permit_x(uid, gid)))
            return -EACCES;
        shared_ptr<Inode> child_from = parent_from.get()->find(paths_from[i]);
        if (child_from == nullptr)
            return -ENOENT;
        parent_from = child_from;
    }
    if (!parent_from.get()->is_dir())
        return -ENOTDIR;
    string last_from = paths_from.back();
    shared_ptr<Inode> child_from = parent_from.get()->find(last_from);
    if (child_from == nullptr)
        return -ENOENT;
    if (child_from.get()->is_dir())
        return -EPERM;
    shared_ptr<Inode> parent_to = root;
    for (u32 i = 0; i < paths_to.size() - 1; i++)
    {
        if (!parent_to.get()->is_dir())
            return -ENOTDIR;
        if (!(parent_to.get()->permit_r(uid, gid) && parent_to.get()->permit_x(uid, gid)))
            return -EACCES;
        shared_ptr<Inode> child_to = parent_to.get()->find(paths_to[i]);
        if (child_to == nullptr)
            return -ENOENT;
        parent_to = child_to;
    }
    if (!parent_to.get()->is_dir())
        return -ENOTDIR;
    if (!parent_to.get()->permit_w(uid, gid))
        return -EACCES;
    string last_to = paths_to.back();
    if (last_to.size() > name_length_limit)
    {
        return -ENAMETOOLONG;
    }
    shared_ptr<Inode> child_to = parent_to.get()->find(last_to);
    if (child_to != nullptr)
        return -EEXIST;
    parent_to.get()->link(last_to, child_from.get()->get_id());
    parent_from.get()->remove(last_from);
    return 0;
}
i64 EasyFileSystem::link(string from, string to)
{
    vector<string> paths_from = split_path(from);
    vector<string> paths_to = split_path(to);
    assert(paths_from.size() > 0 && paths_to.size() > 0);
    shared_ptr<Inode> parent_from = root;
    for (u32 i = 0; i < paths_from.size() - 1; i++)
    {
        if (!parent_from.get()->is_dir())
            return -ENOTDIR;
        if (!(parent_from.get()->permit_r(uid, gid) && parent_from.get()->permit_x(uid, gid)))
            return -EACCES;
        shared_ptr<Inode> child_from = parent_from.get()->find(paths_from[i]);
        if (child_from == nullptr)
            return -ENOENT;
        parent_from = child_from;
    }
    if (!parent_from.get()->is_dir())
        return -ENOTDIR;
    string last_from = paths_from.back();
    shared_ptr<Inode> child_from = parent_from.get()->find(last_from);
    if (child_from == nullptr)
        return -ENOENT;
    if (child_from.get()->is_dir())
        return -EPERM;
    shared_ptr<Inode> parent_to = root;
    for (u32 i = 0; i < paths_to.size() - 1; i++)
    {
        if (!parent_to.get()->is_dir())
            return -ENOTDIR;
        if (!(parent_to.get()->permit_r(uid, gid) && parent_to.get()->permit_x(uid, gid)))
            return -EACCES;
        shared_ptr<Inode> child_to = parent_to.get()->find(paths_to[i]);
        if (child_to == nullptr)
            return -ENOENT;
        parent_to = child_to;
    }
    if (!parent_to.get()->is_dir())
        return -ENOTDIR;
    if (!parent_to.get()->permit_w(uid, gid))
        return -EACCES;
    string last_to = paths_to.back();
    if (last_to.size() > name_length_limit)
    {
        return -ENAMETOOLONG;
    }
    shared_ptr<Inode> child_to = parent_to.get()->find(last_to);
    if (child_to != nullptr)
        return -EEXIST;
    child_from.get()->add_nlink();
    parent_to.get()->link(last_to, child_from.get()->get_id());
    return 0;
}

shared_ptr<Inode> EasyFileSystem::get_inode(u32 inode_id)
{
    u32 inode_block_id, inode_offset;
    get_disk_inode_pos(inode_id, inode_block_id, inode_offset);
    return shared_ptr<Inode>(new Inode(inode_id, inode_block_id, inode_offset, this, block_device));
}

void EasyFileSystem::set_user(u32 _uid, u32 _gid)
{
    uid = _uid;
    gid = _gid;
}

void EasyFileSystem::get_user(u32 &_uid, u32 &_gid)
{
    _uid = uid;
    _gid = gid;
}