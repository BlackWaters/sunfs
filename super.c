#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/backing-dev.h>
#include <linux/sched.h>
#include <linux/parser.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/mm.h>
#include "tools.h"
#include "sunfs.h"
#include "file.h"
#include "sunfs_buddysystem.h"

const struct inode_operations sunfs_inode_file_ops;
const struct inode_operations sunfs_dir_ops;
const struct file_operations sunfs_file_ops;
struct inode *sunfs_alloc_inode(struct super_block *sb);
void sunfs_drop_inode(struct inode *inode);

static const struct address_space_operations sunfs_aops =
    {
        .readpage = simple_readpage,
        .write_begin = simple_write_begin,
        .write_end = simple_write_end,
};

struct inode *sunfs_get_inode(struct super_block *sb, const struct inode *dir, umode_t mode, dev_t dev)
{
    struct inode *inode = new_inode(sb);

    if (inode)
    {
        inode->i_ino = get_next_ino();
        INIT_LIST_HEAD(&inode->i_lru);
        inode_init_owner(inode, dir, mode);
        inode->i_mapping->a_ops = &sunfs_aops;
        //mapping_set_gfp_mask(inode->i_mapping, GFP_HIGHUSER);
        //mapping_set_unevictable(inode->i_mapping);
        inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
        switch (mode & S_IFMT)
        {
        default:
            init_special_inode(inode, mode, dev);
            break;
        case S_IFREG:
            inode->i_op = &sunfs_inode_file_ops;
            inode->i_fop = &sunfs_file_ops;
            break;
        case S_IFDIR:
            inode->i_op = &sunfs_dir_ops;
            inode->i_fop = &simple_dir_operations;
            inc_nlink(inode);
            break;
        case S_IFLNK:
            break;
        }
    }
    return inode;
}

int sunfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev)
{
    struct inode *inode = sunfs_get_inode(dir->i_sb, dir, mode, dev);
    int error = -ENOSPC;
    if (inode)
    {
        d_instantiate(dentry, inode);
        dget(dentry);
        error = 0;
        dir->i_mtime = dir->i_ctime = current_time(dir);
    }
    return error;
}

int sunfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
    printk("Try to mkdir__by sunfs.\n");
    int error = sunfs_mknod(dir, dentry, mode | S_IFDIR, 0);
    if (!error)
        inc_nlink(dir);
    return error;
}

int sunfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
    printk("Try to create a file__by sunfs.\n");
    return sunfs_mknod(dir, dentry, mode | S_IFREG, 0);
}

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

static struct super_operations sunfs_ops =
    {
        .alloc_inode = sunfs_alloc_inode,
        .statfs = simple_statfs,
        .destroy_inode = sunfs_drop_inode,
};

int sunfs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct inode *root;
    int err;

    sb->s_maxbytes = MAX_LFS_FILESIZE;
    sb->s_blocksize = PAGE_SIZE;
    sb->s_blocksize_bits = PAGE_SHIFT;
    sb->s_op = &sunfs_ops;

    printk("Make the root of sunfs.\n");

    root = sunfs_get_inode(sb, NULL, S_IFDIR | 0755, 0);
    sb->s_root = d_make_root(root);
    if (!sb->s_root)
        return -ENOMEM;
    return 0;
}

static struct dentry *sunfs_mount(struct file_system_type *fs_type, int flags, const char *dev_nme, void *data)
{
    printk("Ready to mount sunfs to system.\n");
    return mount_nodev(fs_type, flags, data, sunfs_fill_super);
}

struct kmem_cache *sunfs_inode_cachep;

void InodeCacheInit(void)
{
    //note: we set ctor as NULL, may be incorect.
    sunfs_inode_cachep =
        kmem_cache_create("sunfs_inode_cache", sizeof(struct sunfs_inode_info), 0, 0, NULL);
    if (sunfs_inode_cachep == NULL)
        printk("Inode cache allocs error!\n");
    return;
}

void InodeCacheDestroy(void)
{
    kmem_cache_destroy(sunfs_inode_cachep);
    return;
}

static struct file_system_type sunfs_type =
    {
        .owner = THIS_MODULE,
        .name = "sun_fs",
        .mount = sunfs_mount,
        .kill_sb = kill_litter_super,
        //.fs_flags   =   FS_USERNS_MOUNT,
};

static int __init init_sunfs(void)
{
    //Init sunfs
    InitPageTableForSunfs(PADDR_START, PADDR_END);
    Buddysystem_init();
    InodeCacheInit();
    int err;
    printk("ready to register sunfs.\n");
    err = register_filesystem(&sunfs_type);
    if (!err)
        printk("register success.\n");
    testFunction();
    return err;
}

static void __exit exit_sunfs(void)
{    
    printk("ready to unregister sunfs.\n");
    InodeCacheDestroy();
    unregister_filesystem(&sunfs_type);
}

MODULE_AUTHOR("Ruinan Sun");
MODULE_DESCRIPTION("My first attempt on file system");
MODULE_LICENSE("GPL");
module_init(init_sunfs);
module_exit(exit_sunfs);
