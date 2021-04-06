#ifndef SUNFS_BUDDYSYSTEM_H
#define SUNFS_BUDDYSYSTEM_H
#include <linux/types.h>

extern struct list_head buddy_base[11];
extern struct mutex buddy_lock;

struct sunfs_page
{
    unsigned long vaddr;
    unsigned int order;
    unsigned int num;
    struct list_head list;
};

void ShowFullBuddyList(void);
bool Buddysystem_init(void);
struct sunfs_page *sunfs_getpage(unsigned int order);
void sunfs_freepage(unsigned long vaddr, unsigned int order);
void ZeroFilePage(struct sunfs_page *pg);

#endif