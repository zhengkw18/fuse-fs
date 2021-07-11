#include "easyfs.h"

int main(int argc, char *argv[])
{
  // // ./easyfs /disk 0 -f ...
  // ./easyfs /disk 2 uid gid -f ...
  // ./easyfs /disk 3 uid gid password -f ...
  u32 arg_num = atoi(argv[2]);
  u32 uid = 0, gid = 0;
  if (arg_num >= 2)
  {
    uid = atoi(argv[3]);
    gid = atoi(argv[4]);
  }
  string password = "";
  if (arg_num == 3)
  {
    password = argv[5];
  }
  arg_num++;
  for (u32 i = 2; i < argc - arg_num; i++)
  {
    argv[i] = argv[i + arg_num];
  }
  shared_ptr<EasyFileSystem> efs;
  shared_ptr<BlockDevice> block_device;
  bool create = false;
  for (u32 i = 0; i < device_num; i++)
  {
    FILE *fp = fopen((root_file + to_string(i)).c_str(), "r+");
    if (fp == nullptr)
    {
      int fd = open((root_file + to_string(i)).c_str(), O_RDWR | O_CREAT, 0666);
      ftruncate(fd, device_sz / device_num);
      close(fd);
      create = true;
    }
  }
  block_device = shared_ptr<BlockDevice>(new BlockDevice(password));
  if (create)
  {
    efs = EasyFileSystem::create(block_device);
  }
  else
  {
    efs = EasyFileSystem::open(block_device);
  }

  if (efs == nullptr)
  {
    cout << "Open failed: incorrect password or file corrupted" << endl;
    return -1;
  }
  efs.get()->set_user(uid, gid);
  EasyFS fs(block_device, efs, uid, gid);

  int status = fs.run(argc - arg_num, argv);

  return status;
}
