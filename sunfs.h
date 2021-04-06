#ifndef SUNFS_H
#define SUNFS_H
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/types.h>

#define PADDR_START 0x100000000
#define PADDR_END 0x13fffffff
#define SUNFS_PAGESHIFT 12
#define SUNFS_PAGESIZE ((unsigned long)1 << SUNFS_PAGESHIFT)
#define SUNFS_PAGEMASK (SUNFS_PAGESIZE - 1)
#define PGENTRY_SIZE 512
#define FPMD_SHIFT (21)
#define FPTE_SHIFT SUNFS_PAGESHIFT
#define FPAGE_MASK (PGENTRY_SIZE - 1)
#define DEBUG_FUNCTION static inline 
#define RCUMODE_RW

typedef unsigned long fpmd_t;
typedef unsigned long fpte_t;

struct sunfs_inode_info
{
    struct inode vfs_inode;
    fpmd_t *fpmd;
    unsigned int num_pages;
};

extern struct kmem_cache *sunfs_inode_cachep;
extern atomic_t writer;
extern atomic_t reader;
extern struct mutex writing;

static struct sunfs_inode_info *SUNFS_INIFO(struct inode *ino)
{
    struct sunfs_inode_info *ret = container_of(ino, struct sunfs_inode_info, vfs_inode);
    return ret;
}

#endif
