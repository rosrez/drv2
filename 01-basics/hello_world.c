#include <linux/module.h>
#include <linux/init.h>

static int __init my_init(void)
{
    printk(KERN_INFO "Module loaded at 0x%p\n", my_init);
    return 0;
}

static void __exit my_exit(void)
{
    printk(KERN_INFO "Module unloaded from 0x%p\n", my_exit);
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("MODULE AUTHOR");
MODULE_LICENSE("GPL v2");
