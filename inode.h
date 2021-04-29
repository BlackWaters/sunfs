#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/backing-dev.h>
#include <linux/ramfs.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/parser.h>
#include <linux/magic.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include "sunfs.h"
#include "sunfs_filepgt.h"

struct address_space_operations sunfs_aops =
{
        .readpage = simple_readpage,
        .write_begin = simple_write_begin,
        .write_end = simple_write_end,
};

const struct file_operations sunfs_file_ops =
{
        .read = sunfs_file_read,
        .write = sunfs_file_write,
        .read_iter = generic_file_read_iter,
        .write_iter = generic_file_write_iter,
        .mmap = generic_file_mmap,
        .llseek = generic_file_llseek,
};

const struct inode_operations sunfs_inode_file_ops =
{
        .setattr = simple_setattr,
        .getattr = simple_getattr,
};

const struct inode_operations sunfs_dir_ops =
{
        .create = sunfs_create,
        .lookup = simple_lookup,
        .link = simple_link,
        .unlink = simple_unlink,
        .mkdir = sunfs_mkdir,
        .mknod = sunfs_mknod,
        .rmdir = simple_rmdir,
        .rename = simple_rename,
};

struct inode *sunfs_alloc_inode(struct super_block *sb);
void sunfs_drop_inode(struct inode *inode);
struct inode *sunfs_get_inode(struct super_block *sb, const struct inode *dir, umode_t mode, dev_t dev);
int sunfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev);
int sunfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
int sunfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
