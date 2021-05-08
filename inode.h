#ifndef INODE_H
#define INODE_H

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
#include <linux/rbtree.h>
#include "sunfs.h"
#include "sunfs_filepgt.h"
#include "file.h"

struct sunfs_inotree
{
    struct rb_node node;
    unsigned int l,r;  // [l,r] is a range for this inode table
};

struct inode *sunfs_alloc_inode(struct super_block *sb);
void sunfs_drop_inode(struct inode *inode);
//struct inode *sunfs_get_inode(struct super_block *sb, const struct inode *dir, umode_t mode, dev_t dev);
int sunfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev);
int sunfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
int sunfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
unsigned int sunfs_read_inode(struct inode *inode, struct sunfs_inode *si);
struct inode *sunfs_iget(struct super_block *sb, unsigned int ino);
struct inode *sunfs_new_inode(struct super_block *sb, const struct inode *dir, umode_t mode, dev_t dev);
void sunfs_update_inode(struct inode *inode, struct sunfs_inode *si);
void sunfs_update_time(struct inode *inode);
bool inotree_seg_cross(const struct sunfs_inotree *a, const struct sunfs_inotree *b);
bool sunfs_inode_table_insert(struct rb_root *, struct sunfs_inotree *);
int sunfs_get_newino(void);
bool sunfs_inode_table_init(void);
bool sunfs_free_ino(unsigned int l,unsigned int r);
void sunfs_free_inode(struct sunfs_inode *si);



#endif 
