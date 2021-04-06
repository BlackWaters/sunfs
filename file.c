#include <linux/uaccess.h>
#include <linux/list.h>
#include "file.h"
#include "sunfs.h"
#include "sunfs_filepgt.h"
#include "sunfs_buddysystem.h"
#define lowbit(x) x & -x

struct mutex writing;
atomic_t writer = ATOMIC_INIT(0);
atomic_t reader = ATOMIC_INIT(0);

#ifndef RCUMODE_RW

void InitFilePara(void)
{
    return;
}

ssize_t sunfs_file_read(
    struct file *filp,
    char __user *buf,
    size_t len,
    loff_t *ppos)
{
    struct address_space *mapping = filp->f_mapping;
    struct inode *inode = mapping->host;
    struct sunfs_inode_info *info = SUNFS_INIFO(inode);
    unsigned long start_vaddr;
    ssize_t ret = 0;
    loff_t isize = i_size_read(inode);
    //file page walk
    fpmd_t *fpmd;
    fpte_t *fpte;
    //if we are out of file.
    if (*ppos > isize || info->fpmd == NULL)
    {
        printk("Out of file");
        return 0;
    }

    if (*ppos + len > isize)
        len = isize - *ppos;
    down_read(&inode->i_rwsem);
    while (len)
    {
        fpmd = fpmd_offset(info->fpmd, *ppos);
        if (!*fpmd) // Our file may have hole
        {
            fpte = get_zeroed_page(GFP_KERNEL);
            if (!fpte)
            {
                printk("Can not alloc fpte.");
                goto out_reading;
            }
            *fpmd = fpte;
        }
        fpte = fpte_offset((fpte_t *)*fpmd, *ppos);
        if (!*fpte) // file hole
        {
            struct sunfs_page *pg = sunfs_getpage(0);
            if (!pg)
            {
                printk("Run out of memory, can't fill hole in the file.");
                goto out_reading;
            }
            *fpte = pg->vaddr;
            kfree(pg);
        }
        start_vaddr = *fpte + offset_inpg(*ppos);
        unsigned long copysize = (len < SUNFS_PAGESIZE - offset_inpg(*ppos) ? len : SUNFS_PAGESIZE - offset_inpg(*ppos));
        unsigned long rr = copy_to_user(buf, start_vaddr, copysize);
        if (rr)
        {
            printk("Error in copy_to_user!\n");
            break;
        }
        len -= copysize;
        ret += copysize;
        *ppos += copysize;
        buf += copysize;
    }
out_reading:
    up_read(&inode->i_rwsem);
    file_accessed(filp);
    return ret;
}

ssize_t sunfs_file_write(
    struct file *filp,
    const char __user *buf,
    size_t len,
    loff_t *ppos)
{
    struct address_space *mapping = filp->f_mapping;
    struct inode *inode = mapping->host;
    struct sunfs_inode_info *info = SUNFS_INIFO(inode);
    unsigned long start_vaddr;
    fpmd_t *fpmd;
    fpte_t *fpte;
    loff_t isize = i_size_read(inode);
    ssize_t ret = 0;
    //use read/write sem
    down_write(&inode->i_rwsem);
    if (!info->fpmd)
    {
        info->fpmd = get_zeroed_page(GFP_KERNEL);
        if (!info->fpmd)
        {
            printk("Can not alloc fpmd.");
            goto out_writing;
        }
    }
    while (len)
    {
        fpmd = fpmd_offset(info->fpmd, *ppos);
        if (!*fpmd)
        {
            fpte = get_zeroed_page(GFP_KERNEL);
            if (!fpte)
            {
                printk("Can not alloc fpte.");
                goto out_writing;
            }
            *fpmd = fpte;
        }
        fpte = fpte_offset((fpte_t *)*fpmd, *ppos);
        if (!*fpte)
        {
            struct sunfs_page *pg = sunfs_getpage(0);
            if (!pg)
            {
                printk("Run out of memory.");
                goto out_writing;
            }
            info->num_pages++;
            *fpte = pg->vaddr;
            kfree(pg);
        }

        start_vaddr = *fpte + offset_inpg(*ppos);
        unsigned long copysize = (len < SUNFS_PAGESIZE - offset_inpg(*ppos) ? len : SUNFS_PAGESIZE - offset_inpg(*ppos));
        unsigned long rr = copy_from_user(start_vaddr, buf, copysize);
        if (rr)
        {
            printk("Error in copy_from_user!\n");
            break;
        }
        len -= copysize;
        ret += copysize;
        *ppos += copysize;
        buf += copysize;
    }
out_writing:
    up_write(&inode->i_rwsem);
    if (*ppos > isize) //file hole
        i_size_write(inode, *ppos);
    return ret;
}

#else // USING RCU mode read/write

void InitFilePara(void)
{
    mutex_init(&writing);
}

ssize_t sunfs_file_read(
    struct file *filp,
    char __user *buf,
    size_t len,
    loff_t *ppos)
{
    struct address_space *mapping = filp->f_mapping;
    struct inode *inode = mapping->host;
    struct sunfs_inode_info *info = SUNFS_INIFO(inode);
    unsigned long start_vaddr;
    ssize_t ret = 0;
    loff_t isize = i_size_read(inode);
    //file page walk
    fpmd_t *fpmd;
    fpte_t *fpte;
    //if we are out of file.
    if (*ppos > isize || info->fpmd == NULL)
    {
        printk("Out of file.");
        return 0;
    }

    if (*ppos + len > isize)
        len = isize - *ppos;
    //RCU mode read
    int waiting;
    while (waiting = atomic_read(&writer), waiting)
        ;
    atomic_add(1, &reader);
    while (len)
    {
        fpmd = fpmd_offset(info->fpmd, *ppos);
        if (!*fpmd) // Our file may have hole
        {
            fpte = fpte_offset((fpte_t)*fpmd, *ppos);
            if (!fpte)
            {
                printk("Can not alloc fpte.");
                goto out_reading;
            }
            *fpmd = fpte;
        }
        fpte = fpte_offset((fpte_t *)*fpmd, *ppos);
        if (!*fpte) // file hole
        {
            struct sunfs_page *pg = sunfs_getpage(0);
            if (!pg)
            {
                printk("Run out of memory, can't fill hole in the file.");
                goto out_reading;
            }
            *fpte = pg->vaddr;
            kfree(pg);
        }
        start_vaddr = *fpte + offset_inpg(*ppos);
        unsigned long copysize = (len < SUNFS_PAGESIZE - offset_inpg(*ppos) ? len : SUNFS_PAGESIZE - offset_inpg(*ppos));
        unsigned long rr = copy_to_user(buf, start_vaddr, copysize);
        if (rr)
        {
            printk("Error in copy_to_user!\n");
            break;
        }
        len -= copysize;
        ret += copysize;
        *ppos += copysize;
        buf += copysize;
    }

out_reading:
    atomic_sub(1, &reader);
    file_accessed(filp);
    return ret;
}

ssize_t sunfs_file_write(
    struct file *filp,
    const char __user *buf,
    size_t len,
    loff_t *ppos)
{
    struct address_space *mapping = filp->f_mapping;
    struct inode *inode = mapping->host;
    struct sunfs_inode_info *info = SUNFS_INIFO(inode);
    unsigned long start_vaddr;
    fpmd_t *fpmd = NULL;
    fpte_t *fpte = NULL;
    loff_t isize = i_size_read(inode);
    loff_t offset = *ppos;
    ssize_t ret = 0;

    int startpage = *ppos >> SUNFS_PAGESHIFT;
    int endpage = (*ppos + len - 1) >> SUNFS_PAGESHIFT;
    int need_pages = endpage - startpage + 1;
    unsigned long i = 0;
    struct sunfs_page *pg;
    unsigned int cur_order = 10;
    struct list_head head;

    INIT_LIST_HEAD(&head);
    
    while (need_pages)
    {
        while ((1 << cur_order) > need_pages)
            cur_order--;
        pg = sunfs_getpage(cur_order);
        if (!pg)
        {
            if (cur_order == 0)
            {
                printk("Run out of memory.");
                ret = 0;
                goto free_list;
            }
            cur_order--;
            continue; // reduce order and try again.
        }
        //get page success
        list_add(&pg->list, &head);
        need_pages -= (1 << cur_order);
    }

    //copy data
    list_for_each_entry(pg, &head, list)
    {
        start_vaddr = pg->vaddr + offset_inpg(*ppos);
        unsigned long copysize = len < ((1 << pg->order) * SUNFS_PAGESIZE - offset_inpg(*ppos)) ? len : ((1 << pg->order) * SUNFS_PAGESIZE - offset_inpg(*ppos)); //copy data to every page.
        unsigned long rr = copy_from_user(start_vaddr, buf, copysize);
        if (rr)
        {
            printk("Error in copy_from_user!\n");
            ret = 0;
            *ppos = offset;
            goto free_list;
        }
        *ppos += copysize;
        buf += copysize;
    }
    atomic_add(1, &writer);
    int waiting;
    while (waiting = atomic_read(&reader), waiting)
        ;
    mutex_lock(&writing);
    //change file page table
    //check info->fpmd
    if (!info->fpmd)
    {
        info->fpmd = get_zeroed_page(GFP_KERNEL);
        if (!info->fpmd)
        {
            printk("Can not alloc fpmd."); //Can not get fpmd, just go out and free page
            ret = 0;
            *ppos = offset;
            goto out_writing;
        }
    }
    // Deal with first page
    fpmd = fpmd_offset(info->fpmd, startpage << SUNFS_PAGESHIFT);
    if (!*fpmd)
    {
        fpte = get_zeroed_page(GFP_KERNEL);
        if (!fpte)
        {
            printk("Can not alloc fpte.");
            ret = 0;
            *ppos = offset;
            goto out_writing;
        }
        *fpmd = fpte;
    }
    fpte = fpte_offset((fpte_t *)*fpmd, startpage << SUNFS_PAGESHIFT);
    if (*fpte) //first page is dirty, we should copy data.
    {
        pg = list_entry(head.next, struct sunfs_page, list);
        memcpy((void *)pg->vaddr, (void *)*fpte, offset_inpg(offset));
    }
    //Deal with last page
    fpmd = fpmd_offset(info->fpmd, endpage << SUNFS_PAGESHIFT);
    if (!*fpmd)
    {
        fpte = get_zeroed_page(GFP_KERNEL);
        if (!fpte)
        {
            printk("Can not alloc fpte.");
            ret = 0;
            *ppos = offset;
            goto out_writing;
        }
        *fpmd = fpte;
    }
    fpte = fpte_offset((fpte_t *)*fpmd, endpage << SUNFS_PAGESHIFT);
    if (*fpte) // last page is dirty, we do the same as first page
    {
        pg = list_entry(head.prev, struct sunfs_page, list); // get list tail
        start_vaddr = *fpte + offset_inpg(*ppos - 1) + 1;
        memcpy((void *)(pg->vaddr + (1 << pg->order) * SUNFS_PAGESIZE) - SUNFS_PAGESIZE + offset_inpg(*ppos - 1) + 1, (void *)start_vaddr, SUNFS_PAGESIZE - (offset_inpg(*ppos - 1) + 1));
    }

    //Try to replace file page
    pg = list_entry(head.next, struct sunfs_page, list);
    int ResnumInpg = 1 << pg->order;
    for (i = startpage; i <= endpage; i++)
    {
        fpmd = fpmd_offset(info->fpmd, i << SUNFS_PAGESHIFT);
        if (!*fpmd)
        {
            fpte = get_zeroed_page(GFP_KERNEL);
            if (!fpte)
            {
                printk("Can not alloc fpte.");
                ret = 0;
                *ppos = offset;
                goto out_writing;
            }
            *fpmd = fpte;
        }
        // page table is ok, get file page
        fpte = fpte_offset((fpte_t *)*fpmd, i << SUNFS_PAGESHIFT);

        if (*fpte) //This page is not empty, so we have to free .
        {
            //printk(KERN_ERR "Free page at 0x%lx\n", *fpte);
            sunfs_freepage(*fpte, 0);
        }
        else
            info->num_pages++; // this page is new alloced by us, so we inc the count.
        *fpte = pg->vaddr;
        ResnumInpg--;
        if (ResnumInpg == 0)
        {
            pg = list_next_entry(pg, list);
            if (i != endpage)
                ResnumInpg = 1 << pg->order; // not last page
        }
        else
            pg->vaddr += SUNFS_PAGESIZE;
    }
    // no error, ret should be len
    ret = len;

out_writing:
    mutex_unlock(&writing);
    atomic_sub(1, &writer);

free_list:
    while (!list_empty(&head))
    {
        pg = list_entry(head.next, struct sunfs_page, list);
        list_del(&pg->list);
        if (ret == 0)
            sunfs_freepage(pg->vaddr, pg->order);
        kfree(pg);
    }
   
    if (*ppos > isize)
        i_size_write(inode, *ppos);

    return ret;
}

#endif
