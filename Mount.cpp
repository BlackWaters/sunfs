#include <iostream>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <errno.h>
#include <sys/types.h>
using namespace std;

int main()
{
    if (mount("/dev/sda2", "/root/sun_fs/test/", "sun_fs", MS_NODEV, NULL) != 0)
    {
        printf("Mount error!\n");
        printf("%s\n", strerror(errno));
        return 0;
    }
    return 0;
}
