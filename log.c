#include "log.h"
#include "sunfs_filepgt.h"
#include "inode.h"
#include "sunfs_buddysystem.h"

struct mutex log_lock;
struct kmem_cache *sunfs_log_info_cachep;
const int MAX_INODE_LOG = SUNFS_PAGESIZE / sizeof(struct sunfs_log_entry);

//Must be called with log_lock hold!!!
static inline void writeback_sb_logtail(struct sunfs_super_block *sb, struct sunfs_log_entry *tail)
{
    sb->tail_log = cpu_to_le64(tail); // this should be atomic
}

/*
 * Set log_entry inactive, and reduce super_block logsize.
 * If it succeeds, return 1, otherwise returns 0
 */
inline bool set_sunfs_log_entry_inactive(struct sunfs_log_entry *plog)
{
    mutex_lock(&log_lock);
    plog->active = 0;
    mutex_unlock(&log_lock);
    return 1;
}

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
            if (fpmd_none(fpmd))
            {
                fpte = sunfs_get_onepage();
                if (!fpte)
                {
                    printk(KERN_ERR "Can not alloc page in sunfs!\n");
                    linfo = NULL;
                    goto free_list;
                }
                // *fpmd = cpu_to_le64(fpte);
                fpmd_populate(fpmd, fpte);
            }
            fpte = fpte_offset((fpte_t *)fpmd_vaddr(fpmd), cur_page << SUNFS_PAGESHIFT);
            // *fpte = cpu_to_le64(start_vaddr);
            fpte_populate(fpte, start_vaddr);
            if (cur_page == 0)
                first_page = fpte_vaddr(fpte);
            last_page = fpte_vaddr(fpte);
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
        fpte = fpmd_vaddr(fpmd);
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

void sunfs_log_init(void)
{
    mutex_init(&log_lock);
    sunfs_log_info_cachep =
        kmem_cache_create("sunfs_log_info_cache", sizeof(struct sunfs_log_info), 0, 0, NULL);
    if (sunfs_log_info_cachep == NULL)
        printk("Inode cache allocs error!\n");
}

/*
 * This funciton must be called with log_lock hold
 * It returns the log_tail, which points to an empty log_entry.
 */
struct sunfs_log_entry *sunfs_get_empty_log_entry(struct sunfs_super_block *sb)
{
    struct sunfs_log_entry *tail = (struct sunfs_log_entry *)le64_to_cpu(sb->tail_log);
    struct sunfs_log_entry *ret;

    if (unlikely(tail > __va(PADDR_END)))
        goto checktime;

    while (tail->active)
    {
        tail++;
        if (unlikely(tail > __va(PADDR_END)))
        {
        checktime:
            /*
             * Check log, GC here
             */
            tail = __va(PADDR_START);
        }
    }
    writeback_sb_logtail(sb, tail);

    memset(tail, 0, sizeof(struct sunfs_log_entry));
    return tail;
}

struct sunfs_log_entry *sunfs_get_write_log(
    int file_ino,
    int log_file_ino,
    loff_t offset)
{
    struct sunfs_super_block *sb = sunfs_get_super();
    struct sunfs_log_entry *tail;

    mutex_lock(&log_lock);
    tail = sunfs_get_empty_log_entry(sb);
    if (!tail)
    {
        printk(KERN_ERR "Can not get sunfs_log_entry!\n");
        return NULL;
    }

    tail->log_mode = cpu_to_le16(LOG_WRITE);
    tail->related_ino = cpu_to_le32(file_ino);
    tail->log_file_ino = cpu_to_le64(log_file_ino);
    tail->st_addr = offset;

    //commit
    tail->active = 1;

    tail++;
    writeback_sb_logtail(sb, tail);
    mutex_unlock(&log_lock);

    return tail;
}

/*
 * After finish dir_inode in persistent memory, re-implement this function
 */
struct sunfs_log_entry *sunfs_get_create_log(unsigned int ino)
{
    struct sunfs_super_block *sb = sunfs_get_super();
    struct sunfs_log_entry *tail;

    mutex_lock(&log_lock);
    tail = sunfs_get_empty_log_entry(sb);
    if (!tail)
    {
        printk(KERN_ERR "Can not get sunfs_log_entry!\n");
        return NULL;
    }
    tail->log_mode = cpu_to_le16(LOG_CREATE);
    tail->related_ino = cpu_to_le32(ino);
    tail->active = 1;

    tail++;
    writeback_sb_logtail(sb, tail);

    mutex_unlock(&log_lock);
    return tail;
}

/*
 * Insert log into inode log list
 * Succeed it returns 1, otherwise 0
 */
bool insert_inode_log(struct inode *inode, struct sunfs_log_entry *plog)
{
    struct sunfs_inode_info *info = SUNFS_INIFO(inode);
    if (!info)
    {
        printk(KERN_ERR "Can not get sunfs_inode_info!\n");
        return 0;
    }
    mutex_lock(&info->inode_log_lock);
    if (info->inode_logsize == MAX_INODE_LOG)
    {
        // FIXME: log is full, try to free ...
    }
    unsigned long *ptail = (unsigned long *)(info->tail * 8 + info->log_page);
    printk(KERN_DEBUG "insert: ptail is in 0x%lx\n",ptail);
    *ptail = (unsigned long)plog;
    info->tail++;
    if (unlikely(info->tail) >= MAX_INODE_LOG)
        info->tail %= MAX_INODE_LOG;
    info->inode_logsize++;
    mutex_unlock(&info->inode_log_lock);
    return 1;
}

/*
 * always erase inode log from the head
 * succeed returns 1, otherwise 0
 */
bool erase_inode_log(struct inode *inode)
{
    struct sunfs_inode_info *info = SUNFS_INIFO(inode);
    struct sunfs_log_entry *plog;
    if (!info)
    {
        printk(KERN_ERR "Can not get sunfs_inode_info!\n");
        return 0;
    }
    mutex_lock(&info->inode_log_lock);
    plog = (struct sunfs_log_entry *)(*(unsigned long *)(info->head * 8 + info->log_page));
    printk(KERN_DEBUG "erase: ptail is in 0x%lx\n",plog);
    // check plog
    if (plog->active)
    {
        printk(KERN_ERR "This log entry is still active!\n");
        // something is wrong, return with error and unlock inode_log_lock
        mutex_unlock(&info->inode_log_lock);
        return 0;
    }
    info->head++;
    if (unlikely(info->head) >= MAX_INODE_LOG)
        info->head %= MAX_INODE_LOG;
    info->inode_logsize--;
    mutex_unlock(&info->inode_log_lock);
    return 1;
}