#ifndef SUNFS_FILEPGT_H
#define SUNFS_FILEPGT_H
#include "sunfs.h"

unsigned long *InitFirstPage(void);
fpmd_t *fpmd_offset(fpmd_t *fpmd, unsigned long offset);
fpte_t *fpte_offset(fpte_t *fpte, unsigned long offset);
unsigned long offset_inpg(unsigned long offset);
void append_pages(struct sunfs_inode_info *info, unsigned int page_num);
void delete_pages(struct sunfs_inode_info *info);

#endif