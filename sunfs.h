#ifndef SUNFS_H
#define SUNFS_H
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/types.h>

#define PADDR_START 0x100000000
#define PADDR_END 0x13fffffff
#define INODEZONE_START (PADDR_START + 64)
#define DATAZONE_START (INODEZONE_START + (1 << 23))
#define SUNFS_PAGESHIFT 12
#define SUNFS_PAGESIZE ((unsigned long)1 << SUNFS_PAGESHIFT)
#define SUNFS_PAGEMASK (SUNFS_PAGESIZE - 1)
#define PGENTRY_SIZE 512
#define FPMD_SHIFT (21)
#define FPTE_SHIFT SUNFS_PAGESHIFT
#define FPAGE_MASK (PGENTRY_SIZE - 1)
#define DEBUG_FUNCTION static inline 
#define RCUMODE_RW
#define DIRECTSET

typedef unsigned long fpmd_t;
typedef unsigned long fpte_t;

struct sunfs_inode_info
{
    struct inode vfs_inode;
    fpmd_t *fpmd;
    unsigned int num_pages;
    unsigned int ino;
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

struct sunfs_super_block
{
    __le16		s_magic;            /* magic signature */
	__le32		s_blocksize;        /* blocksize in bytes */

    __le32      free_inode;         
    __le64      StartADDR;
    __le32		s_mtime;            /* mount time */
	__le32		s_wtime;            /* write time */
    __le64      head_log;
    __le64      tail_log;
};

//totoal size of super block is 42 bytes. (64 bytes)

struct sunfs_inode
{
    __le32	i_flags;            /* Inode flags */
	__le64	i_size;             /* Size of data in bytes */
	__le32	i_ctime;            /* Inode modification time */
	__le32	i_mtime;            /* Inode b-tree Modification time */
	__le32	i_dtime;            /* Deletion Time */
    __le16  i_mode;
    
    __le32  i_uid;              /* Owner Uid */
	__le32  i_gid;              /* Group Id */
    __le64  ptr_PMD;             //pmd virtual address
    __le64  pre_logitem;         //pre log item
    u8      active;              //active flag
    u8      reserv;              //reserved 
};

//totoal size of inode is 32 bytes. (32 bytes)

#endif
