#ifndef __BLOCKDEV_H_
#define __BLOCKDEV_H_
#include "utils.h"
#include <pthread.h>
#include <stdio.h>

struct Block
{
    u8 data[block_sz];
};

class BlockDevice
{
    pthread_rwlock_t rwlock[device_num];
    FILE *fp[device_num];
    u8 md[32];

public:
    BlockDevice(string password);
    void read_block(u32 block_id, Block &block);
    void write_block(u32 block_id, const Block &block);
    ~BlockDevice();
};

#endif