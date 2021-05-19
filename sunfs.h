#ifndef SUNFS_H
#define SUNFS_H
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/types.h>

#define PADDR_START 0x100000000UL
#define PADDR_END 0x13fffffffUL
#define INODEZONE_START (PADDR_START + 64)                          //first 64 bytes is super_block
#define DATAZONE_START (INODEZONE_START + ((unsigned long)1 << 23)) //8M inode zone
#define LOGZONE_START (PADDR_END + 1 - ((unsigned long)1 << 21))    //last 2M for log zone
#define SUNFS_PAGESHIFT 12
#define SUNFS_PAGESIZE ((unsigned long)1 << SUNFS_PAGESHIFT)
#define SUNFS_PAGEMASK (SUNFS_PAGESIZE - 1)
#define SUNFS_MAX_FILESIZE ((unsigned long)1 << 30)
#define PGENTRY_SIZE 512
#define FPMD_SHIFT (21)
#define FPTE_SHIFT SUNFS_PAGESHIFT
#define FPAGE_MASK (PGENTRY_SIZE - 1)
#define DEBUG_FUNCTION static inline
//#define DIRECTSET
#define SUNFS_SUPER_MAGIC 0xEFFC
#define SUNFS_ROOT_INO 0

#define RCUMODE_RW
#define USE_LOG

typedef unsigned long fpmd_t;
typedef unsigned long fpte_t;

struct sunfs_inode_info
{
    struct inode vfs_inode;
    fpmd_t *fpmd; // bug!: evrey place we load/store fpmd/fpte should use cpu_to_le..(..)/__le..to_cpu()
    /*
     *  This paramater is defined as number of pages alloced.
     *  Note: for dir_inode, num_pages is set to 0, but it has one pmd for save dentry data.
     */
    unsigned int num_pages;
    unsigned int ino;
    unsigned long log_page;
    unsigned int head;
    unsigned int tail;
    unsigned int inode_logsize;
};

extern struct kmem_cache *sunfs_inode_cachep;
extern atomic_t writer;
extern atomic_t reader;
extern struct mutex writing;

struct sunfs_super_block
{
    __le16 s_magic;     /* magic signature */
    __le32 s_blocksize; /* blocksize in bytes */

    __le32 free_inode;
    __le32 s_mtime; /* mount time */

    __le32 s_wtime; /* write time */

    __le64 StartADDR;
    __le64 tail_log;
};

//totoal size of super block is 48 bytes.

struct sunfs_inode
{
    __le32 i_flags; /* Inode flags */
    __le64 i_size;  /* Size of data in bytes */
    __le32 i_ctime; /* Inode modification time */
    __le32 i_atime; /* Access time */
    __le32 i_dtime; /* Deletion Time */
    __le16 i_mode;

    __le32 i_uid;       /* Owner Uid */
    __le32 i_gid;       /* Group Id */
    __le64 ptr_PMD;     //pmd virtual address
    __le64 pre_logitem; //pre log item
    u8 active;          //active flag
    u8 reserv;          //reserved , now we use this to mark log file
};

//total size of inode is 64 bytes.

static struct sunfs_inode_info *SUNFS_INIFO(struct inode *ino)
{
    struct sunfs_inode_info *ret = container_of(ino, struct sunfs_inode_info, vfs_inode);
    return ret;
}

static struct sunfs_super_block *sunfs_get_super(void)
{
    struct sunfs_super_block *ret = (struct sunfs_super_block *)__va(PADDR_START);
    return ret;
}

static struct sunfs_inode *sunfs_get_inode(unsigned int ino)
{
    struct sunfs_inode *ret;
    ret = (struct sunfs_inode *)__va(INODEZONE_START + ino * 64);
    return ret;
}

#endif
