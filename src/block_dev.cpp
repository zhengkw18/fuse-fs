#include <fcntl.h>
#include <unistd.h>
#include "block_dev.h"
#include "aes128.hpp"
#include "sha3.hpp"
extern int errno;
BlockDevice::BlockDevice(string password)
{
    sha3_256((const u8 *)password.c_str(), password.size(), md);
    for (u32 i = 0; i < device_num; i++)
    {
        fp[i] = fopen((root_file + to_string(i)).c_str(), "r+");
        assert(fp[i] != nullptr);
        pthread_rwlock_init(&rwlock[i], nullptr);
    }
}

BlockDevice::~BlockDevice()
{
    for (u32 i = 0; i < device_num; i++)
    {
        fclose(fp[i]);
        pthread_rwlock_destroy(&rwlock[i]);
    }
}

void BlockDevice::read_block(u32 block_id, Block &block)
{
    assert(block_id < device_block_num);
    u32 device_id = block_id % device_num;
    block_id /= device_num;
    pthread_rwlock_rdlock(&rwlock[device_id]);
    fseek(fp[device_id], block_id * block_sz, SEEK_SET);
    Block new_block;
    fread(&new_block, block_sz, 1, fp[device_id]);
    aes128_cbc_decrypt((u8 *)&new_block, (u8 *)&block, block_sz, md, md + 16);
    pthread_rwlock_unlock(&rwlock[device_id]);
}

void BlockDevice::write_block(u32 block_id, const Block &block)
{
    assert(block_id < device_block_num);
    u32 device_id = block_id % device_num;
    block_id /= device_num;
    pthread_rwlock_wrlock(&rwlock[device_id]);
    Block new_block;
    aes128_cbc_encrypt((u8 *)&block, (u8 *)&new_block, block_sz, md, md + 16);
    fseek(fp[device_id], block_id * block_sz, SEEK_SET);
    fwrite(&new_block, block_sz, 1, fp[device_id]);
    fflush(fp[device_id]);
    pthread_rwlock_unlock(&rwlock[device_id]);
}