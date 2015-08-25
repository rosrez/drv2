#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/init.h>
#include <linux/cdev.h>            
#include <linux/device.h>           
#include <linux/kernel.h>
#include <linux/sched.h>         
#include <linux/fs.h>               
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/poll.h>             /*  poll_wait() */
#include <linux/timer.h>
#include <linux/wait.h>             /*  wait_event() */

#define MODNAME "polld" 

#define MAX_DEVS 10
#define DEF_DEVS 4
#define DEV_SIZE 60

#define BUFSIZE 128

/*  number of devices */
static int numdev = DEF_DEVS;
module_param(numdev, int, S_IRUGO);

/*  timer tick 'base': all our timers have periods that are multiples of tick */
static int tick = HZ;
module_param(tick, int, S_IRUGO);

/*  the dev_t for the first device in our char device region */
static dev_t polld_dev;
/*  the major number associated with the above dev */
static int polld_major = 0; /*  discovered automatically */
static int polld_minor = 0; /*  the first minor version - can be set at compile time */

static struct class *polld_class;

/*  device info structure */
struct dev_info {
    int number;
    wait_queue_head_t wq_read;
    atomic_t ready_to_read;
    int period;
    struct timer_list timer;
    char *buffer;
    int cdev_valid;   
    struct cdev cdev;
};

static struct dev_info dev_info[MAX_DEVS];

static unsigned int polld_poll(struct file *filp, poll_table *wait) {
    struct dev_info *dev = filp->private_data;
    unsigned int revents = 0;

    pr_info("%s: polling dev %d\n", MODNAME, dev->number);

    poll_wait(filp, &dev->wq_read, wait);

    if (atomic_read(&dev->ready_to_read))
        revents |= POLLIN | POLLRDNORM;
    
    pr_info("%s: polling dev %d complete; returning %u\n", MODNAME, dev->number, revents);

    return revents;
}

static ssize_t polld_read(struct file *filp, char __user *buf, size_t count, loff_t *pos) {
    struct dev_info *dev = filp->private_data;

    pr_info("%s: reading dev %d (ready = %d)\n", 
            MODNAME, dev->number, atomic_read(&dev->ready_to_read));

    if (wait_event_interruptible(dev->wq_read, atomic_read(&dev->ready_to_read)) < 0)
        return -ERESTARTSYS;
  
    if (copy_to_user(buf, dev->buffer, 1))  /* TODO: replace with put_user() */
        return -EFAULT;

    atomic_set(&dev->ready_to_read, 0); 

    pr_info("%s: read dev %d complete; returning %d\n", MODNAME, dev->number, 1);

    return 1;
}

static ssize_t polld_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos) {
    return count;   /*  the writer will think it has written all its data */
}

static int polld_open(struct inode *inode, struct file *filp) {
    filp->private_data = container_of(inode->i_cdev, struct dev_info, cdev);
    return 0;
}

static struct file_operations polld_fops = {
    .owner =    THIS_MODULE,
    .read =     polld_read,
    .write =    polld_write,
    .open =     polld_open,
    .poll =     polld_poll,
};

static void polld_timer_func(unsigned long arg) {
    struct dev_info *dev = (struct dev_info *) arg;

    pr_info("%s: timer %d fired\n", MODNAME, dev->number); 

    atomic_set(&dev->ready_to_read, 1);
    wake_up_interruptible(&dev->wq_read);

    mod_timer(&dev->timer, jiffies + dev->period);
}

static void __init polld_init_timer(struct dev_info *dev) {
    atomic_set(&dev->ready_to_read, 0);
    init_waitqueue_head(&dev->wq_read);

    dev->period = (dev->number + 1) * tick;   /*  FIXME: replace by something more flexible */

    init_timer(&dev->timer);

    dev->timer.function = polld_timer_func; 
    dev->timer.data = (unsigned long) dev;
    dev->timer.expires = jiffies + dev->period;

    add_timer(&dev->timer);
}

static void __init polld_init_mem(struct dev_info *dev) {
    char c = 48 + dev->number;
    memset(dev->buffer, c, BUFSIZE);
}

static int __init polld_init_device(struct dev_info *dev, int number) {
    dev_t devt;
    int rc;
    char tmp[64];

    dev->number = number;
    devt = MKDEV(polld_major, dev->number + polld_minor);

    /*  allocate memory */
    dev->buffer = kmalloc(BUFSIZE, GFP_KERNEL);
    if (!dev->buffer) {
        rc = -ENOMEM;
        goto fail;
    }
    polld_init_mem(dev);

    /*  initialize cdev */
    cdev_init(&dev->cdev, &polld_fops);
    rc = cdev_add(&dev->cdev, devt, 1);
    if (rc < 0) {
        pr_info("%s: cdev_add() failed: result code = %d\n", MODNAME, rc); 
        goto fail;
    }
    dev->cdev_valid = 1;

    sprintf(tmp, "%s%d", MODNAME, dev->number);
    /*  create /dev/<devname> device node as well as /sys/devices/virtual/<class>/<devname> */
    device_create(polld_class, NULL, devt, NULL, tmp);
    return 0;

fail:
    kfree(dev->buffer);
    dev->buffer = NULL;
    return rc;
}

static int __init polld_init(void) {
    int rc, i;

    rc = alloc_chrdev_region(&polld_dev, 0, numdev, MODNAME);
    if (rc < 0) {
        pr_info("%s: alloc_chrdev_region() failed: result code = %d\n",
            MODNAME, rc);
        return -EFAULT;
    }
    polld_major = MAJOR(polld_dev);

    polld_class = class_create(THIS_MODULE, "polld");
    if (IS_ERR(polld_class)) {
        rc = PTR_ERR(polld_class);
        goto fail_class_create;
    }

    for (i = 0; i < numdev; i++) {
        rc = polld_init_device(&dev_info[i], i);
        if (rc < 0) {
            pr_info("%s: failed to init device %d: result code = %d\n", 
                MODNAME, rc, i);
            continue;
        }

        polld_init_timer(&dev_info[i]);
        pr_info("%s: device %d ready\n", MODNAME, i);
    }

    pr_info("%s: init complete - device(s) start(s) at major: %d, minor: %d\n", 
            MODNAME, MAJOR(polld_dev), MINOR(polld_dev));

    return 0;

fail_class_create:
    unregister_chrdev_region(polld_dev, numdev);
    return rc;
}

static void __exit polld_clear_device(struct dev_info *dev) {
    /*  remove /dev/<devname> device node and /sys/devices/virtual/<class>/<devname> node */
    device_destroy(polld_class, MKDEV(polld_major, polld_minor + dev->number));

    if (dev->cdev_valid)
        cdev_del(&dev->cdev);
    kfree(dev->buffer);
}

static void __exit polld_exit(void) {
    int i;

    for (i = 0; i < numdev; i++) {
        /* delete the timer, wait for completion if the timer routine is running */
        del_timer_sync(&dev_info[i].timer);

        polld_clear_device(&dev_info[i]);
    }
    class_destroy(polld_class);

    unregister_chrdev_region(polld_dev, numdev);

    pr_info("%s: exit complete\n", MODNAME);
}

module_init(polld_init);
module_exit(polld_exit);

MODULE_DESCRIPTION("Multiple pseudo-devices that implement poll() functionality");
MODULE_LICENSE("GPL v2");
