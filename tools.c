#include "tools.h"
#include <linux/mm_types.h>
#include "sunfs_buddysystem.h"
#include "sunfs.h"
#include "log.h"

unsigned long InitPageTableForSunfs(
    unsigned long paddr_start,
    unsigned long paddr_end)
{
    return SetPgd(paddr_start, paddr_end);
}

void Testmemset(void)
{
    unsigned long i = PADDR_START;
    for (; i < PADDR_START + 3 * SUNFS_PAGESIZE; i++)
    {
        char *p = (char *)__va(i);
        *p = 'a';
        printk("%c", *p);
    }
    printk("Inintiail status.\n");
    ZeroFilePage(__va(PADDR_START + SUNFS_PAGESIZE));
    i = PADDR_START;
    printk("First page.\n");
    for (; i < PADDR_START + SUNFS_PAGESIZE; i++)
    {
        char *p = (char *)__va(i);
        if (*p == 0)
            printk("error");
    }
    printk("Second page.\n");
    for (; i < PADDR_START + 2 * SUNFS_PAGESIZE; i++)
    {
        char *p = (char *)__va(i);
        if (*p != 0)
            printk("error");
    }
    printk("Third page.\n");
    for (; i < PADDR_START + 3 * SUNFS_PAGESIZE; i++)
    {
        char *p = (char *)__va(i);
        if (*p == 0)
            printk("error");
    }
}

void testFunction()
{
    printk("0x%lx\n", current->mm);
    if (current->mm == &init_mm)
        printk("It is same.\n");
    else
        printk("It is not same!\n");
    printk("sunfs_super_block size is %d.\n", sizeof(struct sunfs_super_block));
    printk("sunfs_inode size is %d.\n", sizeof(struct sunfs_inode));
    printk("sunfs_log_entry size if %d\n",sizeof(struct sunfs_log_entry));
    print_for_each_vma(current->mm);
    /* test for buddy system
    ShowFullBuddyList();
    struct sunfs_page *p=sunfs_getpage(0);
    printk("%lx %u %u\n",p->vaddr,p->order,p->num);
    ShowFullBuddyList();
    sunfs_freepage(p->vaddr,p->order);
    kfree(p);
    */
    ShowFullBuddyList();
    return;
}

unsigned long SetPgd( //p4d?
    unsigned long paddr_start,
    unsigned long paddr_end)
{
    int cnt1 = 0, cnt2 = 0;
    struct mm_struct *mm = current->mm;
    unsigned long vaddr, vaddr_end, vaddr_next, paddr_last;
    vaddr = (unsigned long)__va(paddr_start);
    vaddr_end = (unsigned long)__va(paddr_end);

    for (; vaddr < vaddr_end; vaddr = vaddr_next)
    {
        vaddr_next = (vaddr & PGDIR_MASK) + PGDIR_SIZE;
        pgd_t *pgd = pgd_offset(mm, vaddr);
        if (pgd_val(*pgd))
        {
            cnt1++;
            pud_t *pud = (pud_t *)pgd_page_vaddr(*pgd);
            paddr_last = SetPud(pud, __pa(vaddr), __pa(vaddr_end));
            continue;
        }
        cnt2++;
        pud_t *pud = pud_alloc_one(mm, vaddr);
        paddr_last = SetPud(pud, __pa(vaddr), __pa(vaddr_end));
        spin_lock(&mm->page_table_lock);
        p4d_populate(mm, (p4d_t *)pgd, pud);
        spin_unlock(&mm->page_table_lock);
    }
    printk("In Pgd : %d %d\n", cnt1, cnt2);
    __flush_tlb_all();
    return paddr_last;
}

unsigned long SetPud(
    pud_t *pud_page,
    unsigned long paddr_start,
    unsigned long paddr_end)
{
    int cnt1 = 0, cnt2 = 0;
    unsigned long vaddr = (unsigned long)__va(paddr_start);
    unsigned long paddr = paddr_start, paddr_next, paddr_last;
    int i = pud_index(vaddr);
    for (; i < PTRS_PER_PUD; i++, paddr = paddr_next)
    {
        pud_t *pud;
        pmd_t *pmd;
        vaddr = __va(paddr);
        pud = pud_page + pud_index(vaddr);
        paddr_next = (paddr & PUD_MASK) + PUD_SIZE;
        if (paddr >= paddr_end)
        {
            //In 64 bit system, after_bootmem is 1.
            continue;
        }

        if (!pud_none(*pud))
        {
            cnt1++;
            if (!pud_large(*pud))
            {
                pmd = pmd_offset(pud, 0);
                paddr_last = SetPmd(pmd, paddr, paddr_end);
                __flush_tlb_all();
                continue;
            }
            printk("SetPud Strange!\n");
            return 0;
        }
        //alloc new pmd
        cnt2++;
        pmd = pmd_alloc_one(current->mm, vaddr);
        paddr_last = SetPmd(pmd, paddr, paddr_end);
        spin_lock(&current->mm->page_table_lock);
        pud_populate(current->mm, pud, pmd);
        spin_unlock(&current->mm->page_table_lock);
    }
    printk("In Pud : %d %d\n", cnt1, cnt2);
    __flush_tlb_all();
    return 0;
}

unsigned long SetPmd(
    pmd_t *pmd_page,
    unsigned long paddr_start,
    unsigned long paddr_end)
{
    int cnt1 = 0, cnt2 = 0;
    unsigned long vaddr = (unsigned long)__va(paddr_start);
    unsigned long paddr = paddr_start, paddr_next, paddr_last;
    int i = pmd_index(vaddr);
    for (; i < PTRS_PER_PMD; i++, paddr = paddr_next)
    {
        pmd_t *pmd;
        pte_t *pte;
        vaddr = (unsigned long)__va(paddr);
        pmd = pmd_page + pmd_index(vaddr);
        paddr_next = (paddr & PMD_MASK) + PMD_SIZE;
        if (paddr >= paddr_end)
            continue; //Same as SetPud
        if (!pmd_none(*pmd))
        {
            cnt1++;
            if (!pmd_large(*pmd))
            {
                spin_lock(&current->mm->page_table_lock);
                pte = (pte_t *)pmd_page_vaddr(*pmd);
                paddr_last = SetPte(pte, paddr, paddr_end);
                spin_unlock(&current->mm->page_table_lock);
                __flush_tlb_all();
                continue;
            }
            printk("SetPmd Strange!\n");
            return 0;
        }
        cnt2++;
        pte = (pte_t *)__get_free_page(GFP_KERNEL_ACCOUNT | __GFP_ZERO);
        paddr_last = SetPte(pte, paddr, paddr_end);
        spin_lock(&current->mm->page_table_lock);
        pmd_populate_kernel(current->mm, pmd, pte);
        spin_unlock(&current->mm->page_table_lock);
    }
    __flush_tlb_all();
    printk("In pmd : %d %d\n", cnt1, cnt2);
    return 0;
}

unsigned long SetPte(
    pte_t *pte_page,
    unsigned long paddr_start,
    unsigned long paddr_end)
{
    int cnt1 = 0, cnt2 = 0;
    unsigned paddr = paddr_start, paddr_next, paddr_last;
    unsigned vaddr = (unsigned long)__va(paddr), pages = 0;
    int i = pte_index(vaddr);
    pte_t *pte = pte_page + pte_index(vaddr);
    for (; i < PTRS_PER_PTE; i++, paddr = paddr_next, pte++)
    {
        paddr_next = (paddr & PAGE_MASK) + PAGE_SIZE;
        if (paddr >= paddr_end || !pte_none(*pte))
        {
            if (paddr >= paddr_end)
                printk("Finish!\n");
            else
                cnt1++;
            continue;
        }
        cnt2++;
        pages++;
        set_pte(pte, pfn_pte(paddr >> PAGE_SHIFT, PAGE_KERNEL));
        paddr_last = (paddr & PAGE_MASK) + PAGE_SIZE;
    }
    printk("In pte : %d %d\n", cnt1, cnt2);
    return 0;
}

void print_for_each_vma(struct mm_struct *mm)
{
    struct vm_area_struct *vma = NULL, last;

    //vma = find_vma(mm, 0);  //It's the same as vma=mm->mmap
    vma = mm->mmap;
    if (!vma)
    {
        printk("Can not get vma.\n");
        return;
    }

    for (; vma; vma = vma->vm_next)
        printk(" st: 0x%lx ed: 0x%lx length: 0x%lx\n", vma->vm_start, vma->vm_end, vma->vm_end - vma->vm_start);
    return;
}


