#ifndef PAGEINFO_H
#define PAGEINFO_H

#ifndef  __KERNEL__

/* copied from const.h - C versions of _AC/_AT macros */

#define _AT(T,X)    ((T)(X))

/* copied from: arch/x86/include/asm/pgtable_types.h */

#define _PAGE_BIT_PRESENT   0   /* is present */
#define _PAGE_BIT_RW        1   /* writeable */
#define _PAGE_BIT_USER      2   /* userspace addressable */
#define _PAGE_BIT_PWT       3   /* page write through */
#define _PAGE_BIT_PCD       4   /* page cache disabled */
#define _PAGE_BIT_ACCESSED  5   /* was accessed (raised by CPU) */
#define _PAGE_BIT_DIRTY     6   /* was written to (raised by CPU) */
#define _PAGE_BIT_PSE       7   /* 4 MB (or 2MB) page */
#define _PAGE_BIT_PAT       7   /* on 4KB pages */
#define _PAGE_BIT_GLOBAL    8   /* Global TLB entry PPro+ */
#define _PAGE_BIT_UNUSED1   9   /* available for programmer */
#define _PAGE_BIT_IOMAP     10  /* flag used to indicate IO mapping */
#define _PAGE_BIT_HIDDEN    11  /* hidden by kmemcheck */
#define _PAGE_BIT_PAT_LARGE 12  /* On 2MB or 1GB pages */
#define _PAGE_BIT_SPECIAL   _PAGE_BIT_UNUSED1
#define _PAGE_BIT_CPA_TEST  _PAGE_BIT_UNUSED1
#define _PAGE_BIT_NX           63       /* No execute: only valid after cpuid check */

/* If _PAGE_BIT_PRESENT is clear, we use these: */
/* - if the user mapped it with PROT_NONE; pte_present gives true */
#define _PAGE_BIT_PROTNONE  _PAGE_BIT_GLOBAL
/* - set: nonlinear file mapping, saved PTE; unset:swap */
#define _PAGE_BIT_FILE      _PAGE_BIT_DIRTY

#define _PAGE_PRESENT   (_AT(unsigned long, 1) << _PAGE_BIT_PRESENT)
#define _PAGE_RW    (_AT(unsigned long, 1) << _PAGE_BIT_RW)
#define _PAGE_USER  (_AT(unsigned long, 1) << _PAGE_BIT_USER)
#define _PAGE_PWT   (_AT(unsigned long, 1) << _PAGE_BIT_PWT)
#define _PAGE_PCD   (_AT(unsigned long, 1) << _PAGE_BIT_PCD)
#define _PAGE_ACCESSED  (_AT(unsigned long, 1) << _PAGE_BIT_ACCESSED)
#define _PAGE_DIRTY (_AT(unsigned long, 1) << _PAGE_BIT_DIRTY)
#define _PAGE_PSE   (_AT(unsigned long, 1) << _PAGE_BIT_PSE)
#define _PAGE_GLOBAL    (_AT(unsigned long, 1) << _PAGE_BIT_GLOBAL)
#define _PAGE_UNUSED1   (_AT(unsigned long, 1) << _PAGE_BIT_UNUSED1)
#define _PAGE_IOMAP (_AT(unsigned long, 1) << _PAGE_BIT_IOMAP)
#define _PAGE_PAT   (_AT(unsigned long, 1) << _PAGE_BIT_PAT)
#define _PAGE_PAT_LARGE (_AT(unsigned long, 1) << _PAGE_BIT_PAT_LARGE)
#define _PAGE_SPECIAL   (_AT(unsigned long, 1) << _PAGE_BIT_SPECIAL)
#define _PAGE_CPA_TEST  (_AT(unsigned long, 1) << _PAGE_BIT_CPA_TEST)
#define _PAGE_NX        (_AT(unsigned long, 1) << _PAGE_BIT_NX)

#endif /* __kernel */

struct pageinfo {
    unsigned long va;   /* entry param */
    unsigned long va_dir[4];
    unsigned long pa_dir[4];
    unsigned long flags[4];
    unsigned long cr3;
    unsigned long pgd;
    int level;
    int none;
    int prev_majflt;
    int prev_minflt;
    int cur_majflt;
    int cur_minflt;
};

#endif /* PAGEINFO_H */
