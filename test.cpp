#include <iostream>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <errno.h>
#include <sys/types.h>

using namespace std;

void displaypid()
{
    pid_t pid = getpid();
    printf("%u\n", pid);
    getchar();
    return;
}

int main()
{
    displaypid();
    if (mount("/dev/sda2", "/root/sun_fs/test/", "sun_fs", MS_NODEV, NULL) != 0)
    {
        printf("Mount error!\n");
        printf("%s\n", strerror(errno));
        return 0;
    }
    int fd = open("/root/sun_fs/test/testfile", O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd == -1)
    {
        printf("Open file error\n");
        return 0;
    }
    ssize_t err;

    char *buf = (char *)malloc(4096 * 2);
    memset(buf, 0, sizeof(buf));
    //snprintf(buf, 2 * 4096, "This is a test message.");
    for (int i=0;i<1*4096;i++) buf[i]=(i%26)+'a';
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
    if (lseek(fd,0,SEEK_SET)==-1)
    {
        printf("error!\n");
        printf("%s\n", strerror(errno));
    }
    memset(buf, 0, sizeof(buf));
    err = read(fd, buf, 1*4096);
    if (err == -1)
    {
        printf("Read file error\n");
        return 0;
    }
    printf("%d : %s\n", err, buf);
    char *tests = (char*)0UL;
    printf("Direct access from user!\n");
    for (int i=0;i<4096;i++,tests++) printf("%c",*tests);
    printf("\n");

/*
    if (lseek(fd, 4090, SEEK_SET) == -1)
    {
        printf("lseek error\n");
        return 0;
    }
    memset(buf,0,sizeof(buf));
    snprintf(buf,2*4096,"**THIS IS A TEST MESSAGE!**");
    len=strlen(buf);
    err= write(fd,buf,len);
    if (err==-1)
    {
        printf("error!\n");
        printf("%s\n", strerror(errno));
    }
    //read
    if (lseek(fd,0,SEEK_SET)==-1)
    {
        printf("error!\n");
        printf("%s\n", strerror(errno));
    }
    memset(buf, 0, sizeof(buf));
    err = read(fd, buf, 2*4096);
    if (err == -1)
    {
        printf("Read file error\n");
        return 0;
    }
    printf("%d : %s\n", err, buf);
    */
    close(fd);
    return 0;
}
