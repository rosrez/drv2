#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>       /* class_create() etc. */
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/uaccess.h>      /* copy to/from user */

#include <asm/pgtable_64.h>     /* pgd_, pud_, pmd_, pte_ functions */

#include "kmacros.h"
#include "pageinfo.h"
#include "pageinfo_ioctl.h"

#define MODNAME "pageinfo"

static struct class *class;
static dev_t devt;
static struct cdev cdev;

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

static int get_pageinfo(struct __user pageinfo *uinfo)
{
    int present, none;

    pgd_t *pgd;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;

    /* SAFE: spinlock_t *ptl; */
    struct pageinfo info;

    memset(&info, 0, sizeof(struct pageinfo));
    get_user(info.va, &uinfo->va);

    /* the above leads to info.level == 0 */

    info.cr3 = read_cr3();

    pgd = current->mm->pgd;
    info.va_dir[info.level] = (unsigned long) pgd;

    info.level++;
    pud = pud_offset(pgd, info.va);
    info.va_dir[info.level] = (unsigned long) pud;
    info.pa_dir[info.level] = pud_pa(*pud);
    none = pud_none(*pud);
    present = pud_present(*pud);
    if (none || pud_large(*pud))
        goto out;
    info.flags[info.level] = pud_flags(*pud);

    info.level++;
    pmd = pmd_offset(pud, info.va);
    info.va_dir[info.level] = (unsigned long) pmd;
    info.pa_dir[info.level] = pmd_pa(*pmd);
    none = pmd_none(*pmd); 
    present = pmd_present(*pmd);
    if (none || pmd_large(*pmd))
        goto out;
    info.flags[info.level] = pmd_flags(*pmd);

    info.level++;
    /* SAFE: pte = pte_offset_map_lock(current->mm, pmd, info.va, &ptl); */

    pte = pte_offset_kernel(pmd, info.va);

    info.va_dir[info.level] = (unsigned long) pte;
    info.pa_dir[info.level] = pte_pa(*pte);
    present = pte_present(*pte);
    if (present)
        info.flags[info.level] = pte_flags(*pte);
 
    /* SAFE: pte_unmap_unlock(pte, ptl); */

out:
    pr_info("%s: get_pageinfo() -- va = %lx (level: %d, present: %d) :: cr3: %lx -- pgd: %lx\n", 
        MODNAME, info.va, info.level, !!present, info.cr3, info.va_dir[0]);

    info.none = none;

    info.prev_majflt = current->maj_flt;
    info.prev_minflt = current->min_flt;
 
    if (copy_to_user(uinfo, &info, sizeof (struct pageinfo)))
        return -EFAULT;

    info.cur_majflt = current->maj_flt;
    info.cur_minflt = current->min_flt; 

    return 0;
}

static int pi_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    int rc;

    if (_IOC_TYPE(cmd) != PINFO_MAGIC)
        return -EINVAL;

    rc = 0;
    switch (cmd) {

    case (PINFO_GET_ADDR):
        rc = get_pageinfo((struct pageinfo __user *) arg);
        break;

    default:
        rc = -EINVAL;
        break;
    }
    
    return rc;
}

struct file_operations fops = {
    .owner = THIS_MODULE,
    .ioctl = pi_ioctl
};

static int __init pi_init(void)
{
    int rc;

    /* char dev region allocation: deals with devt */
    if ((rc = alloc_chrdev_region(&devt, 0, 1, MODNAME)) < 0)
        err_return(rc, "alloc_chrdev_region() failed\n");

    /* assign fops to our cdev struct */
    cdev_init(&cdev, &fops);

    /* make a relationship between a cdev and devt */
    if ((rc = cdev_add(&cdev, devt, 1)) < 0) {
        pr_info("cdev_add() failed\n");
        goto fail_cdev_add;
    }

    /* create device class for node creation under /dev */
    class = class_create(THIS_MODULE, MODNAME);
    if (IS_ERR(class)) {
        rc = PTR_ERR(class);
        goto fail_class_create;
    }

    /* node creation */
    device_create(class, NULL, devt, "%s", MODNAME);

    pr_info("%s: init\n", MODNAME);
    return 0;

fail_class_create:
    cdev_del(&cdev);

fail_cdev_add:
    unregister_chrdev_region(devt, 1);  /* unregister 1 driver, starting at devt (MAJOR/MINOR) */

    return rc;
}

static void __exit pi_exit(void)
{
    /* remove node */
    device_destroy(class, devt);

    /* remove device class */
    class_destroy(class);

    /* delete device (cdev) */
    cdev_del(&cdev);

    /* unregister device (devt) */
    unregister_chrdev_region(devt, 0);

    pr_info("%s: exit\n", MODNAME);
}

module_init(pi_init);
module_exit(pi_exit);

MODULE_AUTHOR("Oleg Rosowiecki");
MODULE_LICENSE("GPL v2");
