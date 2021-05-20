#include "tools.h"
#include <linux/mm_types.h>
#include "sunfs_buddysystem.h"
#include "sunfs.h"
#include "log.h"

#define PAGE_1G (1UL << 30)
#define ALIGNED_PAGE_1G (~(PAGE_1G - 1))

unsigned long InitPageTableForSunfs(
    unsigned long paddr_start,
    unsigned long paddr_end)
{
    return SetPgd(paddr_start, paddr_end);
}
/*
 * try to find a virtual address space which length is size
 * Now we only find virtual address aligned with 1G
 */
unsigned long try_find_valid_vmstart(struct mm_struct *mm, unsigned long length)
{
    struct vm_area_struct *vma = NULL;

    vma = mm->mmap;
    if (!vma)
    {
        printk("Can not get vma.\n");
        return;
    }
    unsigned long last = 0;
    unsigned long l, r;
    for (; vma; vma = vma->vm_next)
    {
        l = vma->vm_start;
        r = vma->vm_end; // we get a virtual address from l to r [l, r)

    re_align:
        last = (last + PAGE_1G - 1) & ALIGNED_PAGE_1G;
        //printk(" st: 0x%lx ed: 0x%lx length: 0x%lx\n", vma->vm_start, vma->vm_end, vma->vm_end - vma->vm_start);
        if (last >= l && last < r)
        {
            printk("last is in [l,r], error!\n");
            last += PAGE_1G;
            goto re_align;
        }
        if (last < l)
        {
            if (last + length <= l)
            {
                printk("find virtual address: 0x%lx\n", last);
                return last;
            }
            else
                last = r;
        }
    }
    return (~0UL);
}

void replace_page_fpmd(struct mm_struct *mm, fpmd_t *fpmd, unsigned long vaddr_start)
{
    pgd_t *pgd;
    pud_t *pud;

    pgd = pgd_offset(mm, vaddr_start);
    if (!pgd_val(*pgd))
    {
        printk(KERN_DEBUG "PUD is NULL, try to alloc pud!\n");
        pud = pud_alloc_one(mm, vaddr_start);
        spin_lock(&mm->page_table_lock);
        p4d_populate(mm, (p4d_t *)pgd, pud);
        spin_unlock(&mm->page_table_lock);
    }
    __flush_tlb_all();
    //pud = (pud_t *)*pgd;
    pud = (pud_t *)pgd_page_vaddr(*pgd);
    // get pud offset
    pud = pud + pud_index(vaddr_start);
    pud_populate(current->mm,pud,(pmd_t *)fpmd);
    //insert fpmd in pud
    //pud->pud = __pa(fpmd);
    fpte_t *fpte = *fpmd;

    *fpte = __pa(*fpte) | 0x067;
    *fpmd = __pa(*fpmd) | 0x067;
    //pud->pud |= 0x1e7;
    __flush_tlb_all();
    return;
}

unsigned long ShowPyhsicADDR(unsigned long addr)
{
    pgd_t *pgd;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;

    printk(KERN_DEBUG "mm->pgd: 0x%lx\n", current->mm->pgd);
    pgd = pgd_offset(current->mm, addr);
    printk(KERN_DEBUG "pgd: 0x%lx\ncontains: 0x%lx\n", pgd, pgd->pgd);
    pud = pud_offset((p4d_t *)pgd, addr);
    printk(KERN_DEBUG "pud: 0x%lx\ncontains: 0x%lx\n", pud, pud->pud);
    pmd = pmd_offset(pud, addr);
    printk(KERN_DEBUG "pmd: 0x%lx\ncontains 0x%lx\n", pmd, pmd->pmd);
    pte = pte_offset_kernel(pmd, addr);
    printk(KERN_DEBUG "pte: 0x%lx\n pte->pte: 0x%lx\n", pte, pte->pte);

    unsigned long mask = 0x000ffffffffff000UL;
    //printk(KERN_DEBUG "mask : %lx\n",mask);
    unsigned long offset = 0x0000000000000fffUL & addr;
    //printk(KERN_DEBUG "offset : %lx\n",offset);
    unsigned long ret = offset | ((unsigned long)(pte->pte) & mask);
    //printk(KERN_DEBUG "pte address : %lx\n", pte->pte & PAGE_MASK);
    printk(KERN_DEBUG "paddr is 0x%lx\n",ret);
    return ret;
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
    unsigned long ret;
    if (current->mm == &init_mm)
        printk("It is same.\n");
    else
        printk("It is not same!\n");
    printk(KERN_DEBUG "current mm is 0x%lx\n", current->mm);
    printk("sunfs_super_block size is %d.\n", sizeof(struct sunfs_super_block));
    printk("sunfs_inode size is %d.\n", sizeof(struct sunfs_inode));
    printk("sunfs_log_entry size if %d\n", sizeof(struct sunfs_log_entry));
    print_for_each_vma(current->mm);
    ret = try_find_valid_vmstart(current->mm, 1UL << 13);
    if (ret == NAGTIVEUL_MASK)
        printk("Can not find valid virtual address!\n");
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
