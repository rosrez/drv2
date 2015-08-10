#include <linux/module.h>
#include <linux/slab.h>         /* kmalloc() */
#include <linux/mm.h>
#include <linux/mm_types.h>     /* struct mm_struct */
#include <linux/sched.h>        /* current, struct task_struct */
#include <linux/hugetlb.h>      /* pud_huge(), pmd_huge() */
#include <asm/pgtable_64.h>        /* pgd_, pud_, pmd_, pte_ functions */

#define MODNAME "pagetabs"

#define err_return(rc, arg...) \
    do { pr_info(arg); return rc; } while (0)

#define err_return_void(arg...) \
    do { pr_info(arg); return; } while (0)

#define LEVEL_CR3 0
#define LEVEL_PGD 1
#define LEVEL_PUD 2
#define LEVEL_PMD 3
#define LEVEL_PTE 4
#define LEVEL_CNT (5)

char *kbuf;

int pmd_huge(pmd_t pmd)
{
    return !!(pmd_val(pmd) & _PAGE_PSE);
}

int pud_huge(pud_t pud)
{
    return !!(pud_val(pud) & _PAGE_PSE);
}

unsigned long pte_pa(pte_t pte)
{
    return (pte_val(pte) & PTE_PFN_MASK);
}

unsigned long pmd_pa(pmd_t pmd)
{
    return (pmd_val(pmd) & PTE_PFN_MASK);
}

unsigned long pud_pa(pud_t pud)
{
    return (pud_val(pud) & PTE_PFN_MASK);
}

int is_user(unsigned long va)
{
    return (va < PAGE_OFFSET);
}

int is_vmalloc(unsigned long va)
{
    return (is_vmalloc_addr((void *) va));
}

int is_kernel(unsigned long va)
{
    return !is_user(va) && !is_vmalloc(va);
}

unsigned long msk[8] = { 0UL, PGDIR_MASK, PUD_MASK, PMD_MASK, PAGE_MASK };
char *page_size[] = { "N/A", "N/A", "1 GB", "2 MB", "4 KB" };

static unsigned long addr_offset(unsigned long addr, int level)
{
    return addr & ~msk[level];
}

static void display_va(unsigned long va, int level)
{
    unsigned long pgnum = va & msk[level];
    unsigned long pgoff = va & ~msk[level];

    pr_info("(l%d) page num = %lx; offset = %lx\n", level, pgnum, pgoff);
}

static void display_pa(unsigned long pa, int level)
{
    unsigned long pgnum = pa & msk[level];
    unsigned long pgoff = pa & ~msk[level];

    pr_info("(l%d) page frame address = %lx; offset = %lx\n", level, pgnum, pgoff);
    pr_info("PHYSICAL ADDRESS: %lx\n", pa);
}

static void display_ptentry(unsigned long flags, int level)
{
    pr_info("PAGE DATA:\n");
    pr_info("present: %d\n", flags ? 1 : 0);

    /* if page is not present, the remaining flags have no meaning */
    if (!flags)
        return;

    pr_info("page_flags: 0x%lx -- individual flags follow:\n", flags);

    pr_info("global: %x\n", !!(flags & _PAGE_GLOBAL));
    pr_info("dirty: %x\n", !!(flags & _PAGE_DIRTY));
    pr_info("file: %x\n", !!(flags & _PAGE_FILE));
    pr_info("accessed: %x\n", !!(flags & _PAGE_ACCESSED));
    pr_info("writeable: %x\n", !!(flags & _PAGE_RW));
    pr_info("exec: %x\n", !(flags & _PAGE_NX));     /* NOTE: _PAGE_NX stands for NON-executable! */
    pr_info("huge: %x\n", !!(flags & _PAGE_PSE));
}

static int display_pte(struct mm_struct *mm, unsigned long vaddr)
{
    pgd_t *pgd;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    spinlock_t *ptl;
    int level;
    unsigned long paddr, flags;

    pr_info("VIRTUAL ADDRESS: %lx\n", vaddr);
    if (is_kernel(vaddr))
        pr_info("Direct mapping: expected physical address %lx\n", __pa(vaddr));

    level = LEVEL_PGD;
    pgd = pgd_offset(mm, vaddr);

    pr_info("pgd offset = %p\n", pgd);
    if (pgd_none(*pgd))
        err_return(-1, "no entry in PGD\n");

    level++;
    pud = pud_offset(pgd, vaddr);
    pr_info("pud offset = %p (%p + 8 * %lx)\n", pud, pgd, pud_index(vaddr));
    if (pud_none(*pud))
        err_return(-1, "no entry in PUD\n");

    flags = pud_present(*pud) ? (unsigned long) pud_flags(*pud) : 0;
    paddr = pud_pa(*pud) + addr_offset(vaddr, level);
    if (pud_large(*pud)) {
        display_va(vaddr, level);
        display_ptentry(flags, level);
        if (pud_present(*pud))
            display_pa(paddr, level);
        return 0;
    }

    level++;
    pmd = pmd_offset(pud, vaddr);
    pr_info("pmd offset = %p (%p + 8 * %lx)\n", pmd, pud, pmd_index(vaddr));
    if (pmd_none(*pmd))
        err_return(-1, "no entry in PMD\n");

    flags = pmd_present(*pmd) ? (unsigned long) pmd_flags(*pmd) : 0;
    paddr = pmd_pa(*pmd) + addr_offset(vaddr, level);
    if (pmd_large(*pmd)) {
        display_va(vaddr, level);
        display_ptentry(flags, level);
        if (pmd_present(*pmd))
            display_pa(paddr, level);
        return 0;
    }

    level++;
    pte = pte_offset_map_lock(mm, pmd, vaddr, &ptl);

    pr_info("pte offset = %p (%p + 8 * %lx)\n", pte, pmd, pte_index(vaddr));

    flags = pte_present(*pte) ? (unsigned long) pte_flags(*pte) : 0;
    paddr = pte_pa(*pte) + addr_offset(vaddr, level);
    display_va(vaddr, level);
    display_ptentry(flags, level);
    if (pte_present(*pte))
        display_pa(paddr, level);

    pte_unmap_unlock(pte, ptl);
    return 0;
}

static void display_const(void)
{
    pr_info("PAGE_OFFSET: %lx\n", PAGE_OFFSET);
    pr_info("PGDIR_SHIFT: %d -- PGDIR_MASK: %lx\n", PGDIR_SHIFT, PGDIR_MASK);
    pr_info("PUD_SHIFT: %d -- PUD_MASK: %lx\n", PUD_SHIFT, PUD_MASK);
    pr_info("PMD_SHIFT: %d -- PMD_MASK: %lx\n", PMD_SHIFT, PMD_MASK);
    pr_info("PAGE_SHIFT: %d -- PAGE_MASK: %lx\n", PAGE_SHIFT, PAGE_MASK);
    pr_info("========================\n");
}

static void display_ptind(unsigned long addr)
{
    unsigned long idx[5], addr2;

    pr_info("addr = %lx\n", addr);

    addr2 = addr;

    /* 
     * PTRS_PER_XXX is the number of entries in a single paging hierarchy level can contain.
     * Since it is always a power of two, subtracting 1 from a PTRS_PER_XXX value will give
     * us a sequence of 1 bits of the width that is enough to describe this many (PTRS_PER_XXX) entries.
     * In case of x86_64, PTRS_PER_XXX is always 512, which means that 9 bits are required to index
     * each individual level. So PTRS_PER_XXX - 1 will give us a bit mask that we can use to mask out
     * unnecessary (upper) bits of an address.
     *
     * There are inline helper functions to achieve the same result:
     * xxx_index(), where xxx is pgd, pud, pmd or pte.
     * We use explicit calculation to illustrate the topic.
     */

    idx[0] = (addr2 >> PGDIR_SHIFT) & (PTRS_PER_PGD - 1);
    idx[1] = (addr2 >> PUD_SHIFT) & (PTRS_PER_PUD - 1);
    idx[2] = (addr2 >> PMD_SHIFT) & (PTRS_PER_PMD - 1);
    idx[3] = (addr2 >> PAGE_SHIFT) & (PTRS_PER_PTE - 1);
    idx[4] = addr2 & ~PAGE_MASK;

    pr_info("|  PGD   |   PUD   |   PMD   |   PTE   |   OFFSET   |\n");
    pr_info("+--------+---------+---------+---------+------------+\n");
    pr_info("|  %03ld   |   %03ld   |   %03ld   |   %03ld   |    %04ld    | IDX\n",
        idx[0], idx[1], idx[2], idx[3], idx[4]);
    pr_info("========================\n");
}

static int __init ptab_init(void)
{
    struct mm_struct *mm;
    unsigned long addr;
    int rc;
    
    if ((kbuf = kmalloc(PAGE_SIZE, GFP_KERNEL)) == NULL)
        return -ENOMEM;

    pr_info("%s init\n", MODNAME);

    display_const();

    pr_info("'Current process': PID = %d, command = '%s'\n",
            current->pid, current->comm);

    /* 
     * for kernel threads, mm is not set and active_mm is copied from the
     * last process's memory descriptor so that it is possible
     * to get to kernel portion of paget tables 
     * */
    mm = current->mm ? current->mm : current->active_mm;

    pr_info("Process PGD = %p\n", mm->pgd);
#ifdef CFG_INIT_LEVEL4_PGT
    pr_info("Kernel PGD (init_level4_pgd) = %p\n", &init_level4_pgt);
#endif

    addr = (unsigned long) kbuf;
    display_ptind(addr);

    pr_info("Walking PROCESS PGD\n");
    pr_info("PAGE INFO (1)\n");
    rc = display_pte(mm, addr);

    kbuf[0] = 'x';      /* perform a write to see if any fields are updated */

    pr_info("KERNEL IDENTITY MAPPED (on WRITE)\n");
    display_pte(mm, addr);

    return 0;
}

static void __exit ptab_exit(void)
{
    kfree(kbuf);
    pr_info("%s exit\n", MODNAME);
}

module_init(ptab_init);
module_exit(ptab_exit);

MODULE_AUTHOR("Oleg Rosowiecki");
MODULE_LICENSE("GPL v2");
