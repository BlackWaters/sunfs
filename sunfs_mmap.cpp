#include <iostream>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <errno.h>
#include <sys/types.h>

using namespace std;

const char dir[512] = "/mnt/ramdisk/testfile";

int main()
{
    int fd = open(dir, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd == -1)
    {
        printf("Open file error\n");
        return 0;
    }
    ssize_t err;

    char *buf = (char *)malloc(1 << 29);
    //snprintf(buf, 2 * 4096, "This is a test message.");
    for (int i = 0; i < (1 << 29); i++)
        buf[i] = (i % 26) + 'a';
    int len = strlen(buf);
    printf("%d\n", len);
    if (lseek(fd, 0, SEEK_SET) == -1)
    {
        printf("lseek error\n");
        printf("%s\n", strerror(errno));
        return 0;
    }
    err = write(fd, buf, len);
    if (err == -1)
    {
        printf("Write file error\n");
        return 0;
    }
    // read whole message
    if (lseek(fd, 0, SEEK_SET) == -1)
    {
        printf("error!\n");
        printf("%s\n", strerror(errno));
    }
    memset(buf, 0, sizeof(buf));
    err = read(fd, buf, len);
    if (err == -1)
    {
        printf("Read file error\n");
        return 0;
    }
    char *dest = buf, *src = 0UL;
    int length = 1 << 22;
    //printf("%d : %s\n", err, buf);
    for (; length != (1 << 30); length <<= 1)
    {
        system("echo 1 > /proc/sys/vm/drop_caches");
        clock_t t1 = clock();
        memcpy(dest, src, length);
        printf("%dM read time: %.6f\n", length >> 20, (clock() - t1) * 1.0 / CLOCKS_PER_SEC * 1000);
    }

    free(buf);
    return 0;
}