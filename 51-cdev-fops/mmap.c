#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>           /* class_create() */
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>               /*  virt_to_phys() */

#define MODNAME __stringify(KBUILD_BASENAME)

static int capacity = PAGE_SIZE;
module_param(capacity, int, S_IRUGO);

static struct cdev *cdev; 
static dev_t mmap_dev;
static char *buffer;

static struct class *mmapd_class;

static int mmapd_mmap(struct file *file, struct vm_area_struct *vma) 
{
    long int pfn;

    /*  offset 'into' the device (supplied in pages) */
    long int offset = vma->vm_pgoff << PAGE_SHIFT;

    /*  length of the mapping is simply a difference between starting and ending user space addresses  */
    long int len = vma->vm_end - vma->vm_start;

    if (offset + len > capacity)
        return -EINVAL;         /*  can't map beyond device memory */

    /* convert virtual to physical address an shift it appropriately to get the physical page frame number */
    pfn = virt_to_phys(buffer + offset) >> PAGE_SHIFT;

    pr_debug("%s: mapping range: %lx - %lx to physical %lx\n",
        MODNAME, vma->vm_start, vma->vm_end, pfn << PAGE_SHIFT);

    /* 
     * Remap the portion of kernel memory that starts at physical page frame number pfn
     * to userspace. The VFS layer sets up a vma structure with start/end virtual addresses
     * and protection according to mmap() arguments supplied by userspace program. 
     * So here we need only convey the VFS-generated framework to remap_pfn_range().
     */
    if ((remap_pfn_range(vma,
            vma->vm_start,
            pfn,
            len,
            vma->vm_page_prot)) < 0)
        return -EAGAIN;
    return 0;    
}

static ssize_t mmapd_read(struct file *file, char __user *buf, size_t count, loff_t *pos) 
{
    if (*pos >= capacity)
        return 0;   /*  EOF */

    if (*pos + count > capacity)
        count = capacity - *pos;

    if (copy_to_user(buf, buffer + *pos, count))
        return -EFAULT;
    
    *pos += count;
    return count;
}

static ssize_t mmapd_write(struct file *file, const char __user *buf, size_t count, loff_t *pos) 
{
    if (*pos >= capacity)
        return -ENOSPC;

    if (*pos + count > capacity)
        count = capacity - *pos;

    if (copy_from_user(buffer + *pos, buf, count))
        return -EFAULT;

    *pos += count;
    return count;
}

static loff_t mmapd_llseek(struct file *file, loff_t offset, int whence) 
{
    loff_t newpos;

    switch (whence) {
        case SEEK_SET:
            newpos = offset;
            break;
        case SEEK_CUR:
            newpos = file->f_pos + offset;
            break;
        case SEEK_END:
            newpos = capacity + offset;
            break;
        default:
            return -EINVAL;     /*  should never happen */
    }
    if (newpos < 0)
        return -EINVAL;
    file->f_pos = newpos;
    return newpos;
}

static struct file_operations mmapd_fops = {
    .owner =    THIS_MODULE,
    .read =     mmapd_read,
    .write =    mmapd_write,
    .llseek=    mmapd_llseek,
    .mmap =     mmapd_mmap,
};

static int __init mmapd_init(void) 
{
    int rc;

    /* allocate one character device */
    rc = alloc_chrdev_region(&mmap_dev, 0, 1, MODNAME);
    if (rc < 0) {
        pr_debug("%s: alloc_chrdev_region() failed: result code = %d\n",
            MODNAME, rc);
        return -EBUSY;
    }

    mmapd_class = class_create(THIS_MODULE, "mmapd");
    if (IS_ERR(mmapd_class)) {
        rc = PTR_ERR(mmapd_class);
        goto fail_class_create;
    }

    buffer = kmalloc(capacity, GFP_KERNEL);
    if (!buffer) {
        rc = -ENOMEM;
        goto fail_kmalloc;
    }
    memset(buffer, '-', capacity); 

    cdev = cdev_alloc();
    if (!cdev) {
        pr_debug("%s: cdev_alloc() failed\n", MODNAME);
        rc = -ENOMEM;
        goto fail_cdev_alloc;
    }

    cdev->ops = &mmapd_fops;
    rc = cdev_add(cdev, mmap_dev, 1);
    if (rc < 0) {
        pr_debug("%s: cdev_add() failed: result code = %d\n", MODNAME, rc);
        rc = -EBUSY;
        goto fail_cdev_add;
    }

    device_create(mmapd_class, NULL, mmap_dev, NULL, "mmap");

    pr_info("%s: init complete - major: %d, minor: %d\n", 
            MODNAME, MAJOR(mmap_dev), MINOR(mmap_dev));
    return 0;

fail_cdev_add:
    cdev_del(cdev);

fail_cdev_alloc:
    kfree(buffer);

fail_kmalloc:
    class_destroy(mmapd_class);

fail_class_create:
    unregister_chrdev_region(mmap_dev, 1);
    return rc;
}

static void __exit mmapd_exit(void) 
{
    cdev_del(cdev);
    kfree(buffer);
    class_destroy(mmapd_class);
    unregister_chrdev_region(mmap_dev, 1);

    pr_info("%s: exit complete\n", MODNAME);
}

module_init(mmapd_init);
module_exit(mmapd_exit);

MODULE_AUTHOR("Oleg Rosowiecki");
MODULE_DESCRIPTION("Mapping kernel pages to userspace");
MODULE_LICENSE("GPL");
