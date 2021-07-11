#include "bitmap.h"

Bitmap::Bitmap(u32 _start_block_id, u32 _blocks)
{
    start_block_id = _start_block_id;
    blocks = _blocks;
    pthread_mutex_init(&lock, nullptr);
}

Bitmap::~Bitmap()
{
    pthread_mutex_destroy(&lock);
}
i64 Bitmap::alloc(shared_ptr<BlockDevice> device)
{
    pthread_mutex_lock(&lock);
    for (u32 block_id = 0; block_id < blocks; block_id++)
    {
        i64 pos = BLOCK_CACHE_MANAGER
                      .get_block_cache(start_block_id + block_id, device, -1)
                      .get()
                      ->modify_and_sync<BitmapBlock, i64>(0, [block_id](BitmapBlock &bitmap_block) -> i64
                                                          {
                                                              for (u32 bits64_pos = 0; bits64_pos < num_u64_per_block; bits64_pos++)
                                                              {
                                                                  u64 bits64 = bitmap_block.data[bits64_pos];
                                                                  if (bits64 != 0xffffffffffffffffull)
                                                                  {
                                                                      u32 inner_pos = __builtin_ctzll(~bits64);
                                                                      bitmap_block.data[bits64_pos] |= 1ull << inner_pos;
                                                                      return block_id * block_bits + bits64_pos * 64 + inner_pos;
                                                                  }
                                                              }
                                                              return -1;
                                                          });
        if (pos >= 0)
        {
            pthread_mutex_unlock(&lock);
            return pos;
        }
    }
    pthread_mutex_unlock(&lock);
    return -1;
}
void Bitmap::dealloc(shared_ptr<BlockDevice> device, u32 bit)
{
    u32 block_pos = bit / block_bits;
    bit = bit % block_bits;
    u32 bits64_pos = bit / 64;
    u32 inner_pos = bit % 64;
    BLOCK_CACHE_MANAGER
        .get_block_cache(start_block_id + block_pos, device, -1)
        .get()
        ->modify_and_sync<BitmapBlock, i64>(0, [block_pos, bits64_pos, inner_pos](BitmapBlock &bitmap_block) -> i64
                                            {
                                                assert((bitmap_block.data[bits64_pos] & (1ull << inner_pos)) > 0);
                                                bitmap_block.data[bits64_pos] &= ~(1ull << inner_pos);
                                                return 0;
                                            });
}
u32 Bitmap::maximum()
{
    return blocks * block_bits;
}

void Bitmap::clear(shared_ptr<BlockDevice> device)
{
    Block empty;
    memset(empty.data, 0, sizeof(empty));
    for (u32 i = start_block_id; i < start_block_id + blocks; i++)
    {
        device.get()->write_block(i, empty);
    }
}