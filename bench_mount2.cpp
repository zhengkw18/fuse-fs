#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string>
#include <time.h>
#include <thread>

// g++ bench_mount2.cpp -o bench_mount2 -O2 -lpthread

const int len = 8 * 1024 * 1024;
unsigned char buf[len];

int write_times = 1000;

std::string file = "/disk";

void single_thread(int id)
{
    int fd = open((file + "/test" + std::to_string(id)).c_str(), O_RDWR, 0666);
    for (int i = 0; i < write_times; i++)
    {
        lseek(fd, 0, SEEK_SET);
        write(fd, buf, len);
    }
    close(fd);
}
int main(int argc, char **argv)
{
    int num_threads = atoi(argv[1]);
    assert(num_threads <= 3);
    for (int i = 0; i < len; i++)
        buf[i] = i % 256;
    std::thread ths[3];
    int err;
    int fd;
    for (int i = 0; i < num_threads; i++)
    {
        fd = open((file + "/test" + std::to_string(i)).c_str(), O_CREAT, 0666);
        close(fd);
        fd = open((file + "/pad" + std::to_string(3 * i)).c_str(), O_CREAT, 0666);
        close(fd);
        fd = open((file + "/pad" + std::to_string(3 * i + 1)).c_str(), O_CREAT, 0666);
        close(fd);
        fd = open((file + "/pad" + std::to_string(3 * i + 2)).c_str(), O_CREAT, 0666);
        close(fd);
    }
    timespec s, e;

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