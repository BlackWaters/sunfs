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
#include "inode.h"

void get_sunfs_superblock(struct super_block *sb)
{
    struct sunfs_super_block *p_sunfs_sb=__va(PADDR_START);
    
    sb->s_maxbytes = MAX_LFS_FILESIZE;
    sb->s_blocksize = le32_to_cpu(p_sunfs_sb->s_blocksize);
    sb->s_blocksize_bits = PAGE_SHIFT;
}

static struct super_operations sunfs_ops =
    {
        .alloc_inode = sunfs_alloc_inode,
        .statfs = simple_statfs,
        .destroy_inode = sunfs_drop_inode,
};

struct inode *sunfs_iget()
{

}

void 

int sunfs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct inode *root;
    int err;
#ifdef DIRECTSET
    sb->s_maxbytes = MAX_LFS_FILESIZE;
    sb->s_blocksize = PAGE_SIZE;
    sb->s_blocksize_bits = PAGE_SHIFT;
#else
    get_sunfs_superblock(sb);
#endif
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
