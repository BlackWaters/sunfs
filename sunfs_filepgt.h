#ifndef SUNFS_FILEPGT_H
#define SUNFS_FILEPGT_H
#include "sunfs.h"

unsigned long *InitFirstPage(void);
fpmd_t *fpmd_offset(fpmd_t *fpmd, unsigned long offset);
fpte_t *fpte_offset(fpte_t *fpte, unsigned long offset);
unsigned long offset_inpg(unsigned long offset);
void append_pages(struct sunfs_inode_info *info, unsigned int page_num);
void delete_pages(struct sunfs_inode_info *info);
unsigned long sunfs_get_onepage(void);

// if fpmd entry is empty returns 1, otherwise 0
static inline bool fpmd_none(const fpmd_t *fpmd)
{
    return (le64_to_cpu(*fpmd) ? 0 : 1);
}

// if fpte entry is empty returns 1, otherwise 0
static inline bool fpte_none(const fpte_t *fpte)
{
    return (le64_to_cpu(*fpte) ? 0 : 1);
}

// return the fpmd entry contains
static inline unsigned long fpmd_val(const fpmd_t *fpmd)
{
    return __va(le64_to_cpu(*fpmd));
}

// return the fpte entry contains
static inline unsigned long fpte_val(const fpte_t *fpte)
{
    return __va(le64_to_cpu(*fpte));
}

// return the virtual address of fpmd contains(where the fpte virtual address starts)
static inline unsigned long fpmd_vaddr(const fpmd_t *fpmd)
{
    return (fpmd_val(fpmd) & SUNFS_PGENTRYMASK);
}

// return the virtual address of fpte contains(where the file page virtual address starts)
static inline unsigned long fpte_vaddr(const fpte_t *fpte)
{
    return (fpte_val(fpte) & SUNFS_PGENTRYMASK);
}

/* 
 * populate fpmd with fpte entry
 * both are virtual address
 */
static inline void fpmd_populate(fpmd_t *fpmd,const fpte_t *fpte)
{
    *fpmd = cpu_to_le64(__pa(fpte) | 0x067);
}

/* 
 * populate fpte with virtual address
 * fpte is virtual address
 */
static inline void fpte_populate(fpte_t *fpte,const unsigned long vaddr)
{
    *fpte = cpu_to_le64(__pa(vaddr) | 0x067);
}

#endif