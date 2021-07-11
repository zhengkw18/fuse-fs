#include "vfs.h"
#include "efs.h"
Inode::Inode(u32 _inode_id, u32 _block_id, u32 _block_offset, EasyFileSystem *_fs, shared_ptr<BlockDevice> _block_device)
{
    inode_id = _inode_id;
    block_id = _block_id;
    block_offset = _block_offset;
    fs = _fs;
    block_device = _block_device;
}
u32 Inode::get_id()
{
    return inode_id;
}

i64 Inode::find_inode_id(string name, DiskInode &disk_inode)
{
    assert(disk_inode.is_dir());
    u32 file_count = disk_inode.get_size() / dirent_sz;
    DirEntry dirent;
    for (u32 i = 0; i < file_count; i++)
    {
        disk_inode.read_at(dirent_sz * i, dirent.as_bytes_mut(), dirent_sz, block_device, inode_id);
        if (dirent.get_name() == name)
            return dirent.get_inode_number();
    }
    return -1;
}

shared_ptr<Inode> Inode::find(string name)
{
    return modify_disk_inode<shared_ptr<Inode>>([this, name](DiskInode &disk_inode) -> shared_ptr<Inode>
                                                {
                                                    i64 inode_id = this->find_inode_id(name, disk_inode);
                                                    if (inode_id < 0)
                                                        return shared_ptr<Inode>(nullptr);
                                                    u32 block_id, block_offset;
                                                    this->fs->get_disk_inode_pos(inode_id, block_id, block_offset);
                                                    return shared_ptr<Inode>(new Inode(inode_id, block_id, block_offset, this->fs, this->block_device));
                                                });
}

void Inode::increase_size(u32 new_size, DiskInode &disk_inode)
{
    if (new_size <= disk_inode.get_size())
        return;
    u32 blocks_needed = disk_inode.blocks_num_needed(new_size);
    vector<u32> v;
    for (u32 i = 0; i < blocks_needed; i++)
    {
        v.push_back(fs->alloc_data());
    }
    disk_inode.increase_size(new_size, v, block_device);
}

shared_ptr<Inode> Inode::create(string name, DiskInodeType type, u32 uid, u32 gid, u32 mode)
{
    u32 new_inode_id = fs->alloc_inode();
    u32 new_inode_block_id, new_inode_block_offset;
    fs->get_disk_inode_pos(new_inode_id, new_inode_block_id, new_inode_block_offset);
    BLOCK_CACHE_MANAGER
        .get_block_cache(new_inode_block_id, block_device, -1)
        .get()
        ->modify_and_sync<DiskInode, u32>(new_inode_block_offset, [type, uid, gid, mode](DiskInode &new_node)
                                          {
                                              new_node.initialize(type);
                                              new_node.set_uid(uid);
                                              new_node.set_gid(gid);
                                              new_node.set_mode(mode);
                                              return 0;
                                          });
    modify_disk_inode<u32>([this, name, new_inode_id](DiskInode &root_inode)
                           {
                               u32 file_count = root_inode.get_size() / dirent_sz;
                               u32 new_size = (file_count + 1) * dirent_sz;
                               this->increase_size(new_size, root_inode);
                               DirEntry dirent(name, new_inode_id);
                               root_inode.write_at(file_count * dirent_sz, dirent.as_bytes(), dirent_sz, this->block_device, -1);
                               root_inode.add_dirent_num();
                               return 0;
                           });
    return shared_ptr<Inode>(new Inode(new_inode_id, new_inode_block_id, new_inode_block_offset, this->fs, this->block_device));
}

void Inode::remove(string name)
{
    modify_disk_inode<u32>([this, name](DiskInode &root_inode)
                           {
                               u32 file_count = root_inode.get_size() / dirent_sz;
                               DirEntry dirent;
                               for (u32 i = 0; i < file_count; i++)
                               {
                                   root_inode.read_at(dirent_sz * i, dirent.as_bytes_mut(), dirent_sz, block_device, -1);
                                   if (dirent.get_name() == name)
                                   {
                                       DirEntry dirent_empty;
                                       root_inode.write_at(i * dirent_sz, dirent_empty.as_bytes(), dirent_sz, this->block_device, -1);
                                       root_inode.sub_dirent_num();
                                       return 0;
                                   }
                               }
                               return 0;
                           });
}

u32 Inode::read_at(u32 offset, u8 *buf, u32 _size)
{
    return modify_disk_inode<u32>([this, offset, buf, _size](DiskInode &disk_inode) -> u32
                                  { return disk_inode.read_at(offset, buf, _size, this->block_device, this->inode_id); });
}

u32 Inode::write_at(u32 offset, const u8 *buf, u32 _size)
{
    return modify_disk_inode<u32>([this, offset, buf, _size](DiskInode &disk_inode) -> u32
                                  {
                                      this->increase_size(offset + _size, disk_inode);
                                      return disk_inode.write_at(offset, buf, _size, this->block_device, this->inode_id);
                                  });
}

void Inode::clear()
{
    vector<u32> data_blocks_dealloc;
    modify_disk_inode<u32>([this, &data_blocks_dealloc](DiskInode &disk_inode) -> u32
                           {
                               data_blocks_dealloc = disk_inode.clear_size(this->block_device);
                               return 0;
                           });
    for (u32 data_block : data_blocks_dealloc)
        this->fs->dealloc_data(data_block);
}

bool Inode::is_dir()
{
    return read_disk_inode<bool>([](const DiskInode &disk_inode) -> bool
                                 { return disk_inode.is_dir(); });
}

bool Inode::is_file()
{
    return read_disk_inode<bool>([](const DiskInode &disk_inode) -> bool
                                 { return disk_inode.is_file(); });
}

void Inode::sync()
{
    BLOCK_CACHE_MANAGER.flush_inode(inode_id);
}

vector<pair<string, u32>> Inode::ls()
{
    return modify_disk_inode<vector<pair<string, u32>>>([this](DiskInode &disk_inode) -> vector<pair<string, u32>>
                                                        {
                                                            u32 file_count = disk_inode.get_size() / dirent_sz;
                                                            vector<pair<string, u32>> files;
                                                            DirEntry dirent;
                                                            for (u32 i = 0; i < file_count; i++)
                                                            {
                                                                disk_inode.read_at(dirent_sz * i, dirent.as_bytes_mut(), dirent_sz, this->block_device, -1);
                                                                if (dirent.get_name().length() > 0)
                                                                {
                                                                    files.push_back(make_pair(dirent.get_name(), dirent.get_inode_number()));
                                                                }
                                                            }
                                                            return files;
                                                        });
}

u32 Inode::get_nlink()
{
    return read_disk_inode<u32>([](const DiskInode &disk_inode) -> u32
                                { return disk_inode.get_nlink(); });
}

void Inode::add_nlink()
{
    modify_disk_inode<u32>([](DiskInode &disk_inode) -> u32
                           {
                               disk_inode.add_nlink();
                               return 0;
                           });
}

bool Inode::sub_nlink()
{
    return modify_disk_inode<bool>([](DiskInode &disk_inode) -> bool
                                   {
                                       disk_inode.sub_nlink();
                                       if (disk_inode.get_nlink() == 0)
                                           return true;
                                       return false;
                                   });
}

void Inode::rename(string old_name, string new_name)
{
    modify_disk_inode<u32>([this, old_name, new_name](DiskInode &root_inode)
                           {
                               u32 file_count = root_inode.get_size() / dirent_sz;
                               DirEntry dirent;
                               for (u32 i = 0; i < file_count; i++)
                               {
                                   root_inode.read_at(dirent_sz * i, dirent.as_bytes_mut(), dirent_sz, block_device, -1);
                                   if (dirent.get_name() == old_name)
                                   {
                                       DirEntry dirent_new(new_name, dirent.get_inode_number());
                                       root_inode.write_at(i * dirent_sz, dirent_new.as_bytes(), dirent_sz, this->block_device, -1);
                                       return 0;
                                   }
                               }
                               return 0;
                           });
}

void Inode::link(string name, u32 inode_id)
{
    modify_disk_inode<u32>([this, name, inode_id](DiskInode &root_inode)
                           {
                               u32 file_count = root_inode.get_size() / dirent_sz;
                               u32 new_size = (file_count + 1) * dirent_sz;
                               this->increase_size(new_size, root_inode);
                               DirEntry dirent(name, inode_id);
                               root_inode.write_at(file_count * dirent_sz, dirent.as_bytes(), dirent_sz, this->block_device, -1);
                               root_inode.add_dirent_num();
                               return 0;
                           });
}

struct stat Inode::get_stat()
{
    return read_disk_inode<struct stat>([this](const DiskInode &disk_inode) -> struct stat
                                        {
                                            struct stat st;
                                            st.st_atime = disk_inode.get_atime();
                                            st.st_ctime = disk_inode.get_ctime();
                                            st.st_dev = 0;
                                            st.st_gid = disk_inode.get_gid();
                                            st.st_ino = this->inode_id;
                                            st.st_mode = disk_inode.get_mode();
                                            if (disk_inode.is_dir())
                                                st.st_mode |= S_IFDIR;
                                            else
                                                st.st_mode |= S_IFREG;
                                            st.st_mtime = disk_inode.get_ctime();
                                            st.st_nlink = disk_inode.get_nlink();
                                            st.st_rdev = 0;
                                            st.st_size = disk_inode.get_size();
                                            st.st_uid = disk_inode.get_uid();
                                            st.st_blksize = block_sz;
                                            st.st_blocks = disk_inode.total_blocks(st.st_size);
                                            return st;
                                        });
}

u32 Inode::get_dirent_num()
{
    return read_disk_inode<u32>([](const DiskInode &disk_inode) -> u32
                                { return disk_inode.get_dirent_num(); });
}

void Inode::set_mode(u32 mode)
{
    modify_disk_inode<u32>([mode](DiskInode &root_inode)
                           {
                               root_inode.set_mode(mode);
                               return 0;
                           });
}

void Inode::set_owner(u32 uid, u32 gid)
{
    modify_disk_inode<u32>([uid, gid](DiskInode &root_inode)
                           {
                               root_inode.set_uid(uid);
                               root_inode.set_gid(gid);
                               return 0;
                           });
}

void Inode::get_permission(u32 &mode, u32 &uid, u32 &gid)
{
    read_disk_inode<u32>([&mode, &uid, &gid](const DiskInode &disk_inode) -> u32
                         {
                             mode = disk_inode.get_mode();
                             uid = disk_inode.get_uid();
                             gid = disk_inode.get_gid();
                             return 0;
                         });
}

bool Inode::permit_r(u32 _uid, u32 _gid)
{
    return read_disk_inode<bool>([_uid, _gid](const DiskInode &disk_inode) -> bool
                                 { return disk_inode.permit_r(_uid, _gid); });
}
bool Inode::permit_w(u32 _uid, u32 _gid)
{
    return read_disk_inode<bool>([_uid, _gid](const DiskInode &disk_inode) -> bool
                                 { return disk_inode.permit_w(_uid, _gid); });
}
bool Inode::permit_x(u32 _uid, u32 _gid)
{
    return read_disk_inode<bool>([_uid, _gid](const DiskInode &disk_inode) -> bool
                                 { return disk_inode.permit_x(_uid, _gid); });
}

void Inode::set_atime(i64 time)
{
    modify_disk_inode<u32>([time](DiskInode &root_inode)
                           {
                               root_inode.set_atime(time);
                               return 0;
                           });
}
void Inode::set_ctime(i64 time)
{
    modify_disk_inode<u32>([time](DiskInode &root_inode)
                           {
                               root_inode.set_ctime(time);
                               return 0;
                           });
}