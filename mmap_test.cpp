#include <iostream>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include <sys/mman.h>

using namespace std;

const char dir[512] = "/mnt/ramdisk/newfile";

int main()
{
    int fd = open(dir, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd == -1)
    {
        printf("Open file error\n");
        return 0;
    }
    char *buf = (char *)malloc(4096);
    for (int i = 0; i < 4096; i++)
        buf[i] = (i % 26) + 'a';
    ssize_t err;
    if (lseek(fd, 0, SEEK_SET) == -1)
    {
        printf("error!\n");
        printf("%s\n", strerror(errno));
    }
    int ti = 1 << 17;
    while (ti--)
    {
        err = write(fd, buf, 4096);
        if (err == -1)
        {
            printf("Write file error\n");
            return 0;
        }
    }
    //write 512M file

    char *src, *dest = (char *)malloc(1 << 29);
    if (dest==NULL)
    {
        printf("Can not alloc memory\n");
        return 0;
    }
    src = (char *) mmap(NULL, 1 << 29, PROT_READ| PROT_WRITE, MAP_SHARED , fd, 0);
    if (src == (void *)-1)
    {
        printf("mmap error!\n");
        printf("%s\n", strerror(errno));
        return 0;
    }
    unsigned int length = 1 << 22;
    for (; length != (1 << 30); length <<= 1)
    {
        system("echo 1 > /proc/sys/vm/drop_caches");
        clock_t t1 = clock();
        memcpy(dest, src, length);
        printf("%dM read time: %.6f\n", length >> 20, (clock() - t1) * 1.0 / CLOCKS_PER_SEC * 1000);
    }
    return 0;
}
