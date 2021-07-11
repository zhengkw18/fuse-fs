#ifndef __BITMAP_H_
#define __BITMAP_H_

#include "block_cache.h"

struct BitmapBlock
{
    u64 data[num_u64_per_block];
};

class Bitmap
{
    u32 start_block_id;
    u32 blocks;
    pthread_mutex_t lock;

public:
    Bitmap(u32 _start_block_id, u32 _blocks);
    ~Bitmap();
    i64 alloc(shared_ptr<BlockDevice> device);
    void dealloc(shared_ptr<BlockDevice> device, u32 bit);
    u32 maximum();
    void clear(shared_ptr<BlockDevice> device);
};

#endif