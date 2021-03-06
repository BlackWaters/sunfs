#ifndef LOG_H
#define LOG_H

#include "sunfs.h"

/* Sunfs LOG_mode */
#define LOG_CREATE 1
#define LOG_DELETE 2
#define LOG_WRITE 3

struct sunfs_log_info
{
    unsigned int ino;
    unsigned long first_page;
    unsigned long last_page;
};

struct sunfs_log_entry
{
    u8 active;   /* active flag */
    __le16 log_mode; /* Log flag */
    __le32 related_ino;
    __le32 log_file_ino;
    __le64 st_addr;
    __le64 reserved; /* make sure this struct is aligned with 32 bytes */
};

extern struct kmem_cache *sunfs_log_info_cachep;

int sunfs_new_logfile(unsigned int logsize);
void sunfs_log_init(void);
bool sunfs_free_logfile(unsigned int ino);

inline bool set_sunfs_log_entry_inactive(struct sunfs_log_entry *plog);
bool insert_inode_log(struct inode *inode, struct sunfs_log_entry *plog);
bool erase_inode_log(struct inode *inode);


struct sunfs_log_info *sunfs_get_logfile(
    const char __user *buf,
    size_t len,
    loff_t *ppos);

struct sunfs_log_entry *sunfs_get_write_log(
    int file_ino,
    int log_file_ino,
    loff_t offset);

#endif