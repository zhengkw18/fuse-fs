#include "block_cache.h"

BlockCacheManager BLOCK_CACHE_MANAGER;

u32 BlockCache::get_block_id()
{
    return block_id;
}
i32 BlockCache::get_inode_id()
{
    return inode_id;
}

BlockCache::BlockCache(u32 _block_id, shared_ptr<BlockDevice> _device, i32 _inode_id)
{
    block_id = _block_id;
    inode_id = _inode_id;
    device = _device;
    device.get()->read_block(block_id, cache);
    modified = false;
    pthread_rwlock_init(&rwlock, nullptr);
}
void BlockCache::sync()
{
    if (modified)
    {
        modified = false;
        device.get()->write_block(block_id, cache);
    }
}
BlockCache::~BlockCache()
{
    sync();
    pthread_rwlock_destroy(&rwlock);
}

BlockCacheManager::BlockCacheManager()
{
    for (u32 i = 0; i < block_cache_group; i++)
        pthread_mutex_init(&locks[i], nullptr);
}

BlockCacheManager::~BlockCacheManager()
{
    for (u32 i = 0; i < block_cache_group; i++)
        pthread_mutex_destroy(&locks[i]);
}
shared_ptr<BlockCache> BlockCacheManager::get_block_cache(u32 block_id, shared_ptr<BlockDevice> device, i32 inode_id)
{
    u32 group_id = block_id % block_cache_group;
    pthread_mutex_lock(&locks[group_id]);
    for (auto p : queue[group_id])
    {
        if (p.get()->get_block_id() == block_id)
        {
            pthread_mutex_unlock(&locks[group_id]);
            return p;
        }
    }
    if (queue[group_id].size() == block_cache_way)
    {
        for (auto iter = queue[group_id].begin(); iter != queue[group_id].end(); iter++)
        {
            if (iter->use_count() == 1)
            {
                queue[group_id].erase(iter);
                break;
            }
        }
    }
    assert(queue[group_id].size() < block_cache_way);
    shared_ptr<BlockCache> block_cache(new BlockCache(block_id, device, inode_id));
    queue[group_id].push_back(block_cache);
    pthread_mutex_unlock(&locks[group_id]);
    return block_cache;
}
void BlockCacheManager::flush(u32 block_id)
{
    u32 group_id = block_id % block_cache_group;
    pthread_mutex_lock(&locks[group_id]);
    for (auto iter = queue[group_id].begin(); iter != queue[group_id].end(); iter++)
    {
        if (iter->get()->get_block_id() == block_id)
        {
            iter->get()->sync();
            break;
        }
    }
    pthread_mutex_unlock(&locks[group_id]);
}
void BlockCacheManager::flush()
{
    for (u32 group_id = 0; group_id < block_cache_group; group_id++)
    {
        pthread_mutex_lock(&locks[group_id]);
        for (auto iter = queue[group_id].begin(); iter != queue[group_id].end(); iter++)
        {
            iter->get()->sync();
        }
        pthread_mutex_unlock(&locks[group_id]);
    }
}
void BlockCacheManager::flush_inode(u32 inode_id)
{
    for (u32 group_id = 0; group_id < block_cache_group; group_id++)
    {
        pthread_mutex_lock(&locks[group_id]);
        for (auto iter = queue[group_id].begin(); iter != queue[group_id].end(); iter++)
        {
            if (iter->get()->get_inode_id() == (i32)inode_id)
                iter->get()->sync();
        }
        pthread_mutex_unlock(&locks[group_id]);
    }
}