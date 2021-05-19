#include <linux/gfp.h>
#include "sunfs_filepgt.h"
#include "sunfs_buddysystem.h"

DEBUG_FUNCTION void TrytoFindFirstpage(fpmd_t *fpmd)
{
    fpte_t *fpte;
    if (fpmd == NULL)
    {
        printk("fpmd is null!\n");
        return;
    }
    fpmd = fpmd_offset(fpmd, 0);
    fpte = fpte_offset((fpte_t *)*fpmd, 0);
    if (fpte == NULL)
    {
        printk("fpte is NULL!\n");
        return;
    }
    printk("0x%lx\n", *fpte);
    return;
}

unsigned long *InitFirstPage()
{
    return NULL;
}

fpmd_t *fpmd_offset(fpmd_t *fpmd, unsigned long offset)
{
    return fpmd + ((offset >> FPMD_SHIFT) & FPAGE_MASK);
}

fpte_t *fpte_offset(fpte_t *fpte, unsigned long offset)
{
    return fpte + ((offset >> FPTE_SHIFT) & FPAGE_MASK);
}

unsigned long offset_inpg(unsigned long offset)
{
    return offset & (SUNFS_PAGESIZE - 1);
}

void append_pages(struct sunfs_inode_info *info, unsigned int page_num)
{
    unsigned int num = info->num_pages;
    if (!info->fpmd)
    {
        info->fpmd = sunfs_get_onepage();
        if (!info->fpmd)
        {
            printk("Can not alloc fpmd!");
            return;
        }
    }
    fpmd_t *fpmd;
    fpte_t *fpte;
    // Now, we just append page one by one, which is very low speed.
    unsigned int i;
    for (i = 0; i < page_num; i++)
    {
        struct sunfs_page *p = sunfs_getpage(0);
        unsigned long start = p->vaddr;
        kfree(p);
        unsigned long offset = (unsigned long)(num + i) << SUNFS_PAGESHIFT;
        fpmd = fpmd_offset(info->fpmd, offset);
        if (!*fpmd)
        {
            fpte_t *fpte = sunfs_get_onepage();
            if (!fpte)
            {
                printk("Can not alloc fpte!");
                return;
            }
            *fpmd = fpte;
        }
        fpte = fpte_offset(*fpmd, offset);
        *fpte = start;
    }
    info->num_pages += page_num;
    return;
}

void delete_pages(struct sunfs_inode_info *info)
{
    if (!info->num_pages)
        goto out;
    fpmd_t *fpmd = info->fpmd;
    fpte_t *fpte = *fpmd;
    unsigned long i = 0, j = 0;
    for (; i < PGENTRY_SIZE; i++, fpmd++)
    {
        fpte = *fpmd;
        j = 0;
        if (info->num_pages) //we still have page to free
        {
            if (!fpte)
                continue;                         //not in this fpte
            for (; j < PGENTRY_SIZE; j++, fpte++) // check every entry
            {
                if (info->num_pages == 0)
                    break;
                if (*fpte)
                {
                    //printk(KERN_ERR "free page at 0x%lx\n",*fpte);
                    sunfs_freepage(*fpte, 0);
                    *fpte = cpu_to_le64(0); // must set this to 0
                    info->num_pages--;
                }
            }
            sunfs_freepage(*fpmd,0); // free original fpte
            *fpmd = cpu_to_le64(0);
        }
        else
            break;
    }
    if (info->fpmd)
        sunfs_freepage(info->fpmd,0);
out:
    if (info->num_pages)
        printk("delete pages error!");
    else
        printk("delete pages complete.\n");
    return 1;
}

// get one page and return virtual address.
unsigned long sunfs_get_onepage(void)
{
    struct sunfs_page *pg;
    unsigned long ret;

    pg = sunfs_getpage(0);
    if (!pg)
    {
        printk("Can not alloc page for sunfs!\n");
        return 0;
    }
    ret = pg->vaddr;
    kfree(pg);
    return ret;
}
