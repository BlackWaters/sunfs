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

struct inode *sunfs_alloc_inode(struct super_block *sb)
{
    struct sunfs_inode_info *info =
        kmem_cache_alloc(sunfs_inode_cachep, GFP_KERNEL);
    if (info == NULL)
    {
        printk("Error in sunfs_alloc_inode.\n");
        return NULL;
    }
    info->fpmd = InitFirstPage();
    info->num_pages = 0;
    inode_init_once(&info->vfs_inode);
    return &info->vfs_inode;
}

void sunfs_drop_inode(struct inode *inode)
{
    struct sunfs_inode_info *info=SUNFS_INIFO(inode);
    delete_pages(info);
    kmem_cache_free(sunfs_inode_cachep,info);
    return;
}


