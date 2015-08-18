#include <linux/module.h>

#include <linux/init.h>
#include <linux/device.h>           /* devinfo node: class_create(), devinfo_create() */
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/list.h>             /* list functions */
#include <linux/slab.h>             /* kzalloc() */
#include <linux/errno.h>            /* errno codes */

#include <linux/uaccess.h>          /* copy_to/from_user() */

#include "ioctl.h"                  /* our ioctl codes */
#include "kmacros.h"

#define MODNAME "ioctl"

static struct class *class;        /* for devinfo node */
struct cdev cdev;
static dev_t devt;

struct kitem {
    text_t text;
    struct list_head list;
};

static struct list_head *pos;
static struct list_head head = LIST_HEAD_INIT(head);

static int ioc_open(struct inode *inode, struct file *file)
{
    return 0;
}

static void ioc_clear(void)
{
    struct kitem *cur, *next;

    list_for_each_entry_safe(cur, next, &head, list) {
        list_del(&cur->list);
    }
}

static int ioc_add(void __user *buf, size_t size)
{
    struct kitem *item;

    /* allocate memory for a new item */
    item = (struct kitem *) kzalloc(sizeof(struct kitem), GFP_KERNEL);
    if (!item)
        err_return (-ENOMEM, "%s: add -- kmalloc\n", MODNAME);

    /* copy userspace data to the item */
    if (copy_from_user(item->text, buf, size))
        err_return (-EFAULT, "%s: add -- copy_from_user\n", MODNAME);

    /* init list hook and add the item to our list */
    INIT_LIST_HEAD(&item->list);
    list_add(&item->list, pos);

    /* position on this item */
    pos = &item->list;

    pr_info("%s: get -- done\n", MODNAME);
    return 0;
}

static int ioc_get(void __user *buf, size_t size)
{
    struct kitem *kitem;

    if (pos == &head) 
        err_return (-EINVAL, "%s: get -- end of list\n", MODNAME);

    kitem = list_entry(pos, struct kitem, list);
    if (copy_to_user(buf, (const void *) &kitem->text, size))
        err_return (-EFAULT, "%s: get -- copy_to_user\n", MODNAME);

    pr_info("%s: get -- done\n", MODNAME);
    return 0;
}

static void ioc_remove(void) 
{
    struct list_head *prev;

    if (list_empty(&head) || pos == &head)
        err_void("%s: list empty or position is not set\n", MODNAME);

    /* go back one position when the current item is being deleted */
    prev = pos->prev;
    list_del(pos);
    pos = prev;
    
    pr_info("%s: remove -- done\n", MODNAME);
}

static int ioc_hasnext(unsigned long arg)
{
    /* USAPI: put_user(kvar, type *ptr) -- kvar is passed by value, size is determined from 'type' */
    put_user((pos->next != &head), (int __user *) arg);
    pr_info("%s: hasnext -- done\n", MODNAME);
    return 0;
}

static void ioc_next(void)
{
    if (pos->next == &head) {
        pr_info("%s: next -- ignoring (end of list)\n", MODNAME);
    } else {
        pos = pos->next;
        pr_info("%s: next -- done\n", MODNAME);
    }

}

static long ioc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int rc;

    if (_IOC_TYPE(cmd) != IOCTL_MAGIC)
        return -EINVAL;

    /* assume success */
    rc = 0;

    switch (cmd) {
    case IOCTL_REWIND:
        pos = &head;
        pr_info("%s: rewind -- done\n", MODNAME);
        break;

    case IOCTL_NEXT:
        ioc_next();
        break;

    case IOCTL_HASNEXT:
        rc = ioc_hasnext(arg);
        break;

    case IOCTL_CLEAR:
        ioc_clear();
        break;

    case IOCTL_REMOVE:
        ioc_remove();
        break;

    case IOCTL_ADD:
        rc = ioc_add((void __user *) arg, _IOC_SIZE(cmd));
        break;
    
    case IOCTL_GET:
        rc = ioc_get((void __user *) arg, _IOC_SIZE(cmd));
        break;

    default:
        rc = -EINVAL;
        break;
    }

    return rc;
}

struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = ioc_open,
    .unlocked_ioctl = ioc_ioctl 
};

static void __init ioc_setup(void)
{
    pos = &head;
}

static int __init ioc_init(void)
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

    ioc_setup();

    pr_info("%s: Initialization complete\n", MODNAME);
    return 0;

fail_class_create:
    cdev_del(&cdev);

fail_cdev_add:
    unregister_chrdev_region(devt, 1);  /* unregister 1 driver, starting at devt (MAJOR/MINOR) */

    return rc;
}

static void __exit ioc_exit(void)
{
    /* remove node */
    device_destroy(class, devt);

    /* remove device class */
    class_destroy(class);

    /* delete device (cdev) */
    cdev_del(&cdev);

    /* unregister device (devt) */
    unregister_chrdev_region(devt, 0);

    ioc_clear();

    pr_info("%s: exited", MODNAME);
}

module_init(ioc_init);
module_exit(ioc_exit);

MODULE_AUTHOR("OR");
MODULE_LICENSE("GPL v2");
