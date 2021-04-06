#ifndef FILE_H
#define FILE_H

ssize_t sunfs_file_read(
    struct file *filp,
    char __user *buf,
    size_t len,
    loff_t *);

ssize_t sunfs_file_write(
    struct file *filp,
    const char __user *buf,
    size_t len,
    loff_t *ppos);

void InitFilePara(void);

#endif