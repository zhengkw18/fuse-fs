#include "efs.h"
#include <thread>
const u32 len = 8 * 1024 * 1024;
u8 buf[len];

shared_ptr<EasyFileSystem> efs;

int write_times = 1000;

void single_thread(int id)
{
    i32 err;
    shared_ptr<Inode> file = efs.get()->find("/test" + to_string(id), err);
    assert(file != nullptr);
    for (int i = 0; i < write_times; i++)
    {
        file.get()->write_at(0, buf, len);
    }
}
int main(int argc, char **argv)
{
    int num_threads = atoi(argv[1]);
    assert(num_threads <= 4);
    for (u32 i = 0; i < device_num; i++)
    {
        FILE *fp = fopen((root_file + to_string(i)).c_str(), "r+");
        if (fp == nullptr)
        {
            int fd = open((root_file + to_string(i)).c_str(), O_RDWR | O_CREAT, 0666);
            ftruncate(fd, device_sz / device_num);
            close(fd);
        }
    }
    shared_ptr<BlockDevice> block_device(new BlockDevice(""));
    efs = EasyFileSystem::create(block_device);
    assert(efs != nullptr);
    for (u32 i = 0; i < len; i++)
        buf[i] = i % 256;
    std::thread ths[4];

    timespec s, e;

    i32 err;
    for (int i = 0; i < num_threads; i++)
    {
        efs.get()->create("/test" + to_string(i), DiskInodeType::File, err, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
        efs.get()->create("/pad" + to_string(3 * i), DiskInodeType::File, err, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
        efs.get()->create("/pad" + to_string(3 * i + 1), DiskInodeType::File, err, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
        efs.get()->create("/pad" + to_string(3 * i + 2), DiskInodeType::File, err, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
    }
    clock_gettime(CLOCK_REALTIME, &s);
    for (int i = 0; i < num_threads; ++i)
    {
        ths[i] = std::thread(single_thread, i);
    }

    for (int i = 0; i < num_threads; ++i)
    {
        ths[i].join();
    }
    clock_gettime(CLOCK_REALTIME, &e);

    double us = (e.tv_sec - s.tv_sec) * 1000000 + (double)(e.tv_nsec - s.tv_nsec) / 1000;
    printf("thread number %d, throughput %lf MB/s\n", num_threads, 1ull * num_threads * write_times * len / 1024 / 1024 * 1000000 / us);
    return 0;
}