#include "log.h"
#include "sunfs_filepgt.h"
#include "inode.h"
#include "sunfs_buddysystem.h"

struct mutex log_lock;
struct kmem_cache *sunfs_log_info_cachep;

int sunfs_new_logfile(unsigned int logsize) //number of pages
{
    unsigned int ino;
    unsigned long vaddr;
    struct list_head head;
    struct sunfs_page *pg;
    int ret;

    ino = sunfs_get_newino();
    if (ino == -1)
    {
        printk("Can not create log file!\n");
        return -1;
    }
    struct sunfs_inode *si = NULL;
    si = sunfs_get_inode(ino);
    if (!si)
    {
        printk("Can not get sunfs_inode!\n");
        return -1;
    }
    ret = ino;
    //init this sunfs_inode, log_file do not have VFS struct

    si->i_mode = cpu_to_le16(S_IFREG);
    si->reserv = 1; //mark this file is log file
    vaddr = sunfs_get_onepage();
    if (!vaddr)
    {
        printk(KERN_ERR "we have no page in sunfs!\n");
        return -1;
    }
    si->ptr_PMD = cpu_to_le64(vaddr);
    si->i_size = logsize;

    return ret;
}

/*
 * This function is designed mainly for write.
 * We only copy data pages in this function.
 */
struct sunfs_log_info *sunfs_get_logfile(
    const char __user *buf,
    size_t len,
    loff_t *ppos)
{
    unsigned long start_vaddr;
    unsigned long end_vaddr;
    unsigned long first_page, last_page;
    struct sunfs_inode *si;
    fpmd_t *si_fpmd = NULL;
    fpmd_t *fpmd = NULL;
    fpte_t *fpte = NULL;
    loff_t offset = *ppos;
    unsigned int ino;
    struct sunfs_log_info *linfo = NULL;

    int startpage = *ppos >> SUNFS_PAGESHIFT;
    int endpage = (*ppos + len - 1) >> SUNFS_PAGESHIFT;
    int need_pages = endpage - startpage + 1;
    unsigned long i = 0;
    struct sunfs_page *pg;
    unsigned int cur_order = 10;
    struct list_head head;

    linfo = kmem_cache_alloc(sunfs_log_info_cachep, GFP_KERNEL);
    if (!linfo)
    {
        printk(KERN_ERR "Can not alloc sunfs_log_info!\n");
        return NULL;
    }

    // get logfile ino
    ino = sunfs_new_logfile(len);
    if (ino == -1)
    {
        printk(KERN_ERR "Can not get logfile ino.\n");
        return NULL;
    }

    //cpoy data from user buffer
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
                linfo = NULL;
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
            linfo = NULL;
            *ppos = offset;
            goto free_list;
        }
        *ppos += copysize;
        buf += copysize;
    }

    si = sunfs_get_inode(ino);
    if (!si)
    {
        printk(KERN_ERR "Can not get sunfs_inode!\n");
        linfo = NULL;
        goto free_list;
    }
    si_fpmd = le64_to_cpu(si->ptr_PMD);
    if (!si_fpmd)
    {
        printk(KERN_ERR "Lose fpmd in sunfs_inode!\n");
        linfo = NULL;
        goto free_list;
    }
    unsigned int cur_page = 0;
    list_for_each_entry(pg, &head, list)
    {
        start_vaddr = pg->vaddr;
        end_vaddr = start_vaddr + (1 << pg->order) * SUNFS_PAGESIZE;
        for (; start_vaddr != end_vaddr; start_vaddr += SUNFS_PAGESIZE)
        {
            fpmd = fpmd_offset(si_fpmd, cur_page << SUNFS_PAGESHIFT);
            if (!*fpmd)
            {
                fpte = sunfs_get_onepage();
                if (!fpte)
                {
                    printk(KERN_ERR "Can not alloc page in sunfs!\n");
                    linfo = NULL;
                    goto free_list;
                }
                *fpmd = cpu_to_le64(fpte);
            }
            fpte = fpte_offset((fpte_t *)le64_to_cpu(*fpmd), cur_page << SUNFS_PAGESHIFT);
            *fpte = cpu_to_le64(start_vaddr);
            if (cur_page == 0)
                first_page = *fpte;
            last_page = *fpte;
            cur_page++;
        }
    }
    linfo->ino = ino;
    linfo->first_page = first_page;
    linfo->last_page = last_page;

free_list:
    while (!list_empty(&head))
    {
        pg = list_entry(head.next, struct sunfs_page, list);
        list_del(&pg->list);
        if (linfo == NULL)
            sunfs_freepage(pg->vaddr, pg->order);
        kfree(pg);
    }

    return linfo;
}

/*
 * Free the space of log file
 * If succeed, it return 1, else 0
 */
bool sunfs_free_logfile(unsigned int ino)
{
    struct sunfs_inode *si;
    fpmd_t *si_fpmd, *fpmd;
    fpte_t *fpte;
    unsigned int i = 0;
    bool ret;
    si = sunfs_get_inode(ino);
    if (!si)
    {
        printk(KERN_ERR "Can not get sunfs_inode!\n");
        return 0;
    }
    si_fpmd = le64_to_cpu(si->ptr_PMD);
    if (!si_fpmd)
    {
        printk(KERN_ERR "Lose fpmd in log file!\n");
        return 0;
    }

    for (fpmd = si_fpmd, i = 0; i < PGENTRY_SIZE; i++, fpmd++)
    {
        fpte = le64_to_cpu(*fpmd);
        if (!fpte)
            break;
        sunfs_freepage(fpte, 0);
    }
    if (si_fpmd)
        sunfs_freepage(si_fpmd, 0);
    // free the using of ino
    ret = sunfs_free_ino(ino, ino);
    if (!ret)
    {
        printk(KERN_ERR "Can not free logfile ino!\n");
        return 0;
    }
    return 1;
}

static inline void writeback_sb_logtail(struct sunfs_super_block *sb, struct sunfs_log_entry *tail)
{
    sb->tail_log = cpu_to_le64(tail); // this should be atomic
}

static inline void writeback_sb_loghead(struct sunfs_super_block *sb, struct sunfs_log_entry *head)
{
    sb->head_log = cpu_to_le64(head);
}

void sunfs_log_init(void)
{
    mutex_init(&log_lock);
    sunfs_log_info_cachep =
        kmem_cache_create("sunfs_log_info_cache", sizeof(struct sunfs_log_info), 0, 0, NULL);
    if (sunfs_log_info_cachep == NULL)
        printk("Inode cache allocs error!\n");
}

struct sunfs_log_entry *sunfs_get_write_log(
    int file_ino,
    int log_file_ino,
    loff_t offset)
{
    //must hold lock
    mutex_lock(&log_lock);
    struct sunfs_super_block *sb = sunfs_get_super();
    struct sunfs_log_entry *tail = (struct sunfs_log_entry *)le64_to_cpu(sb->tail_log);
    struct sunfs_log_entry *head = (struct sunfs_log_entry *)le64_to_cpu(sb->head_log);
    struct sunfs_log_entry *ret;

    while (tail->active)
    {
        if (tail == head)
        {
            /* log is full filled, try to free.
             * Try to check log here.
             */
        }
        else
        {
            tail++;
            if (unlikely(tail > __va(PADDR_END)))
                tail = __va(PADDR_START);
            writeback_sb_logtail(sb, tail);
        }
    }

    memset(tail, 0, sizeof(struct sunfs_log_entry));
    tail->log_mode = cpu_to_le16(LOG_WRITE);
    tail->related_ino = cpu_to_le32(file_ino);
    tail->log_file_ino = cpu_to_le64(log_file_ino);
    tail->st_addr = offset;

    ret = tail;
    //commit
    tail->active = 1;
    tail++;
    if (unlikely(tail > __va(PADDR_END)))
        tail = __va(PADDR_START);
    writeback_sb_logtail(sb, tail);
    mutex_unlock(&log_lock);

    return ret;
}
