#ifndef __BLOCKCACHE_H_
#define __BLOCKCACHE_H_

#include "block_dev.h"

class BlockCache
{
    Block cache;
    u32 block_id;
    i32 inode_id;
    shared_ptr<BlockDevice> device;
    bool modified;
    pthread_rwlock_t rwlock;
    template <typename T>
    const T &get_ref(u32 offset)
    {
        assert(offset + sizeof(T) <= block_sz);
        return *(T *)&cache.data[offset];
    }

    template <typename T>
    T &get_mut(u32 offset)

    {
        assert(offset + sizeof(T) <= block_sz);
        modified = true;
        return *(T *)&cache.data[offset];
    }

public:
    u32 get_block_id();
    i32 get_inode_id();
    BlockCache(u32 _block_id, shared_ptr<BlockDevice> _device, i32 _inode_id);

    template <typename T, typename V>
    V read(u32 offset, function<V(const T &)> f)
    {
        pthread_rwlock_rdlock(&rwlock);
        V v = f(get_ref<T>(offset));
        pthread_rwlock_unlock(&rwlock);
        return v;
    }
    template <typename T, typename V>
    V modify(u32 offset, function<V(T &)> f)
    {
        pthread_rwlock_wrlock(&rwlock);
        V v = f(get_mut<T>(offset));
        pthread_rwlock_unlock(&rwlock);
        return v;
    }

    template <typename T, typename V>
    V modify_and_sync(u32 offset, function<V(T &)> f)
    {
        pthread_rwlock_wrlock(&rwlock);
        V v = f(get_mut<T>(offset));
        sync();
        pthread_rwlock_unlock(&rwlock);
        return v;
    }

    void sync();
    ~BlockCache();
};

class BlockCacheManager
{
    vector<shared_ptr<BlockCache>> queue[block_cache_group];
    pthread_mutex_t locks[block_cache_group];

public:
    BlockCacheManager();
    ~BlockCacheManager();
    shared_ptr<BlockCache> get_block_cache(u32 block_id, shared_ptr<BlockDevice> device, i32 inode_id);
    void flush(u32 block_id);
    void flush();
    void flush_inode(u32 inode_id);
};

extern BlockCacheManager BLOCK_CACHE_MANAGER;

#endif