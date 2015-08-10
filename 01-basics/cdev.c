#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>       /* device node creation, management */
#include <linux/uaccess.h>

#define MODNAME "cdev"

#define err_return(code, message) do {\
    printk(KERN_DEBUG message ": error code = %d\n", code);\
    return code;\
} while (0)

#define err_out(fmt, arg...) do {\
    printk(KERN_DEBUG fmt "\n", arg);\
} while (0)

#define MAX_DEV 10

#define TEXT_SIZE 256

static int major, devcnt; 

module_param(major, int, 0);
module_param(devcnt, int, 0);

struct class *class;

struct devinfo {
    int number;
    int valid;
    char text[TEXT_SIZE];
    struct cdev cdev; 
};

static struct devinfo devinfo[MAX_DEV];

static int cdriver_open(struct inode *inode, struct file *file)
{
    file->private_data = container_of(inode->i_cdev, struct devinfo, cdev);
    return 0;
}

static int cdriver_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t cdriver_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    ssize_t to_copy;
    struct devinfo *dev = (struct devinfo *) file->private_data;

    to_copy = strlen(dev->text);  

    if (*pos >= to_copy)
        return 0;

    to_copy -= *pos;
    if (to_copy > count)
        to_copy = count;

    if (copy_to_user(buf, dev->text, to_copy))
        return -EFAULT;

    *pos += to_copy;
    return to_copy;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = cdriver_open,
    .release = cdriver_release,
    .read = cdriver_read,
};

static int __init cdriver_init_device(int num)
{
    dev_t devt;
    int rc;
    char nodename[32];
    struct devinfo *dev = &devinfo[num];

    dev->number = num;

    cdev_init(&dev->cdev, &fops);

    devt = MKDEV(major, num);

    /* we are now ready to service requests; safe to call cdev_add() */
    if ((rc = cdev_add(&dev->cdev, devt, 1)) < 0)
        err_return(rc, "cdev_add() failed");

    /* cdev_del() must be called for devices that have been successfully added */
    dev->valid = 1;

    /* create device node in /dev */
    sprintf(nodename, "%s%d", MODNAME, dev->number);
    device_create(class, NULL, devt, NULL, "%s%d", MODNAME, dev->number);

    /* create a static text per device */
    sprintf(dev->text, "Response from device %d\n", dev->number);
    return 0;
}

static int __init cdriver_init(void)
{
    dev_t   dev;
    int     rc, i;

    /* default to one device/node */
    if (!devcnt)
        devcnt = 1;

    /* if major number is set, try to register the region starting at major.0 */
    if (major) {
        dev = MKDEV(major, 0);
        if ((rc = register_chrdev_region(dev, devcnt, MODNAME)) < 0)
            err_return(rc, "register_chrdev_region() failed");

    /* most code should use dynamic allocation to avoid conflicts */
    } else {
        if ((rc = alloc_chrdev_region(&dev, 0, devcnt, MODNAME)) < 0)
            err_return(rc, "alloc_chrdev_region() failed");

        major = MAJOR(dev);
    }

    /* create device class for subsequent node creation under /dev */
    class = class_create(THIS_MODULE, "cdev");
    if (IS_ERR(class)) {
        rc = PTR_ERR(class);
        goto fail_class_create;
    }

    for (i = 0; i < devcnt; i++) {
        if (cdriver_init_device(i) < 0) 
           err_out("failed to initialize device %d: error code = %d", i, rc);
    }

    printk(KERN_DEBUG "%s: initialization complete\n", MODNAME);
    return 0;

fail_class_create:
    unregister_chrdev_region(dev, devcnt);
    return rc;
}

static void cdriver_clear_device(struct devinfo *dev)
{
    /* remove device node */
    device_destroy(class, MKDEV(major, dev->number));

    if (dev->valid)
        cdev_del(&dev->cdev);
}

static void __exit cdriver_exit(void)
{
    int i;

    for (i = 0; i < devcnt; i++)
        cdriver_clear_device(&devinfo[i]);

    /* destroy device class (device nodes need it) */
    class_destroy(class);

    /* unregister character device(s) */
    unregister_chrdev_region(MKDEV(major, 0), devcnt);

    printk(KERN_DEBUG "%s: completed exit\n", MODNAME);
}

module_init(cdriver_init);
module_exit(cdriver_exit);

MODULE_DESCRIPTION("Generic character device module");
MODULE_LICENSE("GPL v2");
