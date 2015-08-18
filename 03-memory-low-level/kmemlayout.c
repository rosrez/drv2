#include <linux/module.h>
#include <linux/slab.h>
#include <linux/version.h>

/* architecture specific includes */

#include <asm/page.h>

#define MODNAME "memlayout"

#define MB (1024*1024)

static __init int kml_init(void)
{
    printk(KERN_INFO "%s: initializing\n", MODNAME);
    printk(KERN_INFO "%s\n", __stringify(KBUILD_BASENAME));

    printk(KERN_INFO "PAGE_SHIFT: %d\n", PAGE_SHIFT);
    printk(KERN_INFO "PAGE_OFFSET: %lx\n", PAGE_OFFSET);
    printk(KERN_INFO "PAGE_SIZE: %ld\n", PAGE_SIZE);
    printk(KERN_INFO "=================\n");


    printk(KERN_INFO "__PHYSICAL_START: %x (in MB): %d\n", __PHYSICAL_START, __PHYSICAL_START / MB);
    printk(KERN_INFO "__START_KERNEL_map: %lx\n", __START_KERNEL_map);
    printk(KERN_INFO "__START_KERNEL: %lx\n", __START_KERNEL);
    printk(KERN_INFO "-----------------\n");

    printk(KERN_INFO "VMALLOC_START: %lx\n", VMALLOC_START);
    printk(KERN_INFO "VMALLOC_END: %lx\n", VMALLOC_END);
    printk(KERN_INFO "VMALLOC area size (MB): %ld\n", (VMALLOC_END - VMALLOC_START) / MB);
    printk(KERN_INFO "=================\n");

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,34)
    printk(KERN_INFO "KERNEL_IMAGE_START: %lx\n", KERNEL_IMAGE_START);
    printk(KERN_INFO "KERNEL_IMAG_SIZE (MB): %d\n", KERNEL_IMAGE_SIZE / MB);
    printk(KERN_INFO "=================\n");
#endif
    printk(KERN_INFO "MODULES_VADDR: %lx\n", MODULES_VADDR);
    printk(KERN_INFO "MODULES_END: %lx\n", MODULES_END);
    printk(KERN_INFO "MODULES_LEN (MB): %ld\n", MODULES_LEN / MB);
    printk(KERN_INFO "=================\n");

    return 0;
}

static __exit void kml_exit(void)
{
    printk(KERN_INFO "%s: exiting\n", MODNAME);
}

module_init(kml_init);
module_exit(kml_exit);

MODULE_AUTHOR("Oleg Rosowiecki");
MODULE_LICENSE("GPL v2");
