#ifndef TOOLS_H
#define TOOLS_H
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/sched.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>
#include <linux/spinlock.h>

unsigned long InitPageTableForSunfs(
    unsigned long paddr_start,
    unsigned long paddr_end);

void testFunction(void);

unsigned long SetPgd( //p4d?
    unsigned long paddr_start,
    unsigned long paddr_end);

unsigned long SetPud(
    pud_t *pud_page,
    unsigned long paddr_start,
    unsigned long paddr_end);

unsigned long SetPmd(
    pmd_t *pmd_page,
    unsigned long paddr_start,
    unsigned long paddr_end);

unsigned long SetPte(
    pte_t *pte_page,
    unsigned long paddr_start,
    unsigned long paddr_end);

void print_for_each_vma(struct mm_struct *mm);

#endif