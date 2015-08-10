#ifndef __MODULE_GENERIC_CDEV_INC
#define __MODULE_GENERIC_CDEV_INC

#if 0
#include <linux/module.h>
#include <linux/cdev.h>
#endif

static int __init cdev_setup(struct cdev *cdev, int major, struct file_operations *fops) {
    int result;
    dev_t devnum = MKDEV(major, 0);

    cdev_init(cdev, fops);
    cdev->owner = THIS_MODULE;      // the only additional setup needed after cdev_init

    // we are now ready to handle data; call cdev_add();
    result = cdev_add(cdev, devnum, 1);
    if (result < 0)
        cdev->ops = NULL;       // this will signal uninitialized struct to exit handler
    return result;
}

static int __init generic_chrdev_register(int *major_number, char *name) {
    dev_t dev;
    int result;

    result = alloc_chrdev_region(&dev, 0, 1, name);
    if (result < 0)
        return result;
    *major_number = MAJOR(dev);
    return 0;
}

static void __exit generic_chrdev_exit(int major_number, struct cdev *cdev) {
    dev_t dev;

    if (cdev != NULL && cdev->ops)
        cdev_del(cdev);

    dev = MKDEV(major_number, 0);
    unregister_chrdev_region(dev, 1);
}

#endif  // __MODULE_GENERIC_CDEV

