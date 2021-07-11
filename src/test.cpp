#include "efs.h"
const u32 len = 8 * 1024 * 1024;
u8 buf[len];
u8 buf2[len];
int main()
{
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
    {
        shared_ptr<BlockDevice> block_device(new BlockDevice(""));
        shared_ptr<EasyFileSystem> efs = EasyFileSystem::create(block_device);
        assert(efs != nullptr);
        i32 err;
        shared_ptr<Inode> file = efs.get()->create("/test", DiskInodeType::File, err, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
        assert(file != nullptr);
        for (u32 i = 0; i < len; i++)
            buf[i] = i % 256;
        file.get()->write_at(0, buf, len);
        u32 len1 = file.get()->read_at(0, buf2, len);
        assert(len1 == len);
        for (u32 i = 0; i < len; i++)
            assert(buf2[i] == i % 256);
    }
    {
        shared_ptr<BlockDevice> block_device(new BlockDevice(""));
        shared_ptr<EasyFileSystem> efs = EasyFileSystem::open(block_device);
        assert(efs != nullptr);
        i32 err;
        shared_ptr<Inode> file = efs.get()->find("/test", err);
        assert(file != nullptr);
        u32 len1 = file.get()->read_at(0, buf2, len);
        assert(len1 == len);
        for (u32 i = 0; i < len; i++)
            assert(buf2[i] == i % 256);
    }
    cout << "test 2-level index ok." << endl;
    return 0;
}