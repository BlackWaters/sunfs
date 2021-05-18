#include <linux/list.h>
#include <linux/types.h>
#include <linux/slab.h>
#include "sunfs_buddysystem.h"
#include "sunfs.h"

struct list_head buddy_base[11];
struct mutex buddy_lock;
unsigned long BUDDY_SYSTEM_START;

unsigned int getpagenum(struct sunfs_page *page)
{
    unsigned long vaddr = page->vaddr;
    unsigned int order = page->order;
    // we get page from DATAZONE
    return (unsigned int)((__pa(vaddr) - BUDDY_SYSTEM_START) >> (order + SUNFS_PAGESHIFT));
}

// must be called with buddy_lock hold!!
static inline bool Find_buddy(struct sunfs_page *p)
{
    struct sunfs_page *tmp;
    unsigned int order = p->order;
    list_for_each_entry(tmp, &buddy_base[order], list)
    {
        if ((tmp->num ^ 1) == p->num)
        {
            //update p
            p->order++;
            p->vaddr &= tmp->vaddr;
            p->num = getpagenum(p);
            //remove buddy tmp
            list_del(&tmp->list);
            kfree(tmp);
            return 1;
        }
    }
    return 0;
}

void ShowFullBuddyList(void)
{
    printk(KERN_ALERT "***\n");
    mutex_lock(&buddy_lock);
    unsigned int cur = 0;
    for (; cur <= 10; cur++)
    {
        if (list_empty(&buddy_base[cur]))
        {
            printk("order : %u is empty\n", cur);
            continue;
        }
        printk("order : %u\n", cur);
        struct sunfs_page *p;
        list_for_each_entry(p, &buddy_base[cur], list)
        {
            printk("%lx %u %u\n", p->vaddr, p->order, p->num);
        }
    }
    printk(KERN_ALERT "***\n");
    mutex_unlock(&buddy_lock);
    return;
}

bool Buddysystem_init(void)
{
    mutex_init(&buddy_lock);
    int i = 0;
    for (i = 0; i <= 10; i++)
    {
        INIT_LIST_HEAD(buddy_base + i);
        //printk("%lx\n",buddy_base+i);
    }
    BUDDY_SYSTEM_START = __va(DATAZONE_START);
    unsigned long vaddr = __va(DATAZONE_START);
    unsigned long PAGE_4M = ((unsigned long)1 << 22);
    unsigned long PAGE_4K = ((unsigned long)1 << 12);
    unsigned long PAGE_NOW = PAGE_4M;
    unsigned long order = 10;

    if (LOGZONE_START & SUNFS_PAGEMASK)
    {
        printk(KERN_ERR "End virtual address is not aligned with 4K!\n");
        return 0;
    }

    // we skip some address, make sure start virtual address is aligned with 4K.
    if (vaddr & SUNFS_PAGEMASK)
    {
        vaddr = (vaddr + SUNFS_PAGESIZE) & (~SUNFS_PAGEMASK);
        BUDDY_SYSTEM_START = vaddr;
    }

    while (vaddr != __va(LOGZONE_START))
    {
        /* 
         * Note: we are READY to init page from vaddr to vaddr + PAGE_NOW
         * [vaddr, vaddr + PAGE_NOW)
         * Check this page is correct before init.
         */
        while (vaddr + PAGE_NOW > __va(LOGZONE_START))
        {
            if (order == 0)
                return 0; // error! we can't init buddy system because page is not alligned with 4K!
            // reset vaddr
            PAGE_NOW >>= 1;
            order--;
        }

        for (; vaddr < __va(LOGZONE_START); vaddr += PAGE_NOW)
        {
            struct sunfs_page *p = kmalloc(sizeof(struct sunfs_page), GFP_KERNEL);
            if (p == NULL)
            {
                printk("kmalloc error!\n");
                return 0;
            }
            p->vaddr = vaddr;
            p->order = order;
            p->num = getpagenum(p);
            INIT_LIST_HEAD(&p->list);
            list_add(&p->list, &buddy_base[order]);
        }
    }
    return 1;
}

struct sunfs_page *sunfs_getpage(unsigned int order)
{
    mutex_lock(&buddy_lock);
    unsigned int cur = order;
    for (; cur <= 10; cur++)
    {
        if (!list_empty(&buddy_base[cur]))
            goto found;
    }
    mutex_unlock(&buddy_lock);
    return NULL;
found:;
    struct sunfs_page *p = list_entry(buddy_base[cur].next, struct sunfs_page, list);
    list_del(&p->list);
    if (cur == order)
        goto find_out;
    else
    {
        while (cur > order)
        {
            //init for prev one
            struct sunfs_page *prev_page = kmalloc(sizeof(struct sunfs_page), GFP_KERNEL);
            prev_page->vaddr = p->vaddr;
            prev_page->order = cur - 1;
            prev_page->num = getpagenum(prev_page);
            INIT_LIST_HEAD(&prev_page->list);
            list_add(&prev_page->list, &buddy_base[cur - 1]);
            p->order--;
            p->vaddr += ((unsigned long)1 << (cur - 1 + SUNFS_PAGESHIFT));
            cur--;
        }
        p->num = getpagenum(p);
    }
find_out:
    mutex_unlock(&buddy_lock);
    ZeroFilePage(p); // Zeroing this page before we alloc it.
    return p;
    //must free this node!
}

void sunfs_freepage(unsigned long vaddr, unsigned int order)
{
    struct sunfs_page *p = kmalloc(sizeof(struct sunfs_page), GFP_KERNEL);
    p->vaddr = vaddr;
    p->order = order;
    ZeroFilePage(p);
    p->num = getpagenum(p);
    INIT_LIST_HEAD(&p->list);
    unsigned int cur = order;
    //hold lock
    mutex_lock(&buddy_lock);
    while (cur < 10)
    {
        if (!Find_buddy(p))
        {
            list_add(&p->list, &buddy_base[p->order]);
            break;
        }
        cur++;
    }
    if (cur == 10)
        list_add(&p->list, &buddy_base[p->order]);
    mutex_unlock(&buddy_lock);
    return;
}

//zero this page, this function will be used in delete_pages
void ZeroFilePage(struct sunfs_page *pg)
{
    memset((void *)pg->vaddr, 0, (1 << pg->order) * SUNFS_PAGESIZE);
    return;
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