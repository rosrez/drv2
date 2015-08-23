#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/sched.h>        /*  current, TASK_INTERRUPTIBLE (used by underlying macros) */
#include <linux/wait.h>         /*  wait_event() family */
#include <linux/uaccess.h>      /*  copy_to_user() */

#define MODNAME __stringify(KBUILD_BASENAME)

#define WAKEUP_ALL          1
#define WAKEUP_EXCLUSIVE    2
#define WAKEUP_NR           3

static int wakeup = WAKEUP_ALL;
module_param(wakeup, int, S_IRUGO);

static int nr = 1;
module_param(nr, int, S_IRUGO);

static struct cdev *cdev; 
static dev_t waitev_dev;

static DECLARE_WAIT_QUEUE_HEAD(wq);
static atomic_t write_flag = ATOMIC_INIT(0);
static pid_t write_pid; 

static ssize_t waitev_read(struct file *filp, char __user *buf, size_t count, loff_t *pos) {
    char tmpbuf[128];

    switch (wakeup) {
       case WAKEUP_EXCLUSIVE:
            if (wait_event_interruptible_exclusive(wq, atomic_read(&write_flag)))
                return -ERESTARTSYS;
            break;
        default:
            if (wait_event_interruptible(wq, atomic_read(&write_flag)))
                return -ERESTARTSYS;
            break;
    }

    count = sprintf(tmpbuf, "process %ld woken up by %ld\n", (long) current->pid, (long) write_pid);
    count -= copy_to_user(buf, tmpbuf, count);

    /*  simulate contention by delaying execution to make sure all readers are woken up */
    schedule_timeout(jiffies + HZ);

    atomic_set(&write_flag, 0);     /*  a race condition here, if all readers are woken up */
    return count;  
}

static ssize_t waitev_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos) {
    write_pid = current->pid;
    atomic_set(&write_flag, 1);
    switch (wakeup) {
        case WAKEUP_ALL:
        case WAKEUP_EXCLUSIVE:
            wake_up_interruptible(&wq);
            break;
        case WAKEUP_NR:
            wake_up_nr(&wq, nr);
    }
    *pos += count;
    return count;   /*  the writer will think it has written all its data */
}

static struct file_operations waitev_fops = {
    .owner =    THIS_MODULE,
    .read =     waitev_read,
    .write =    waitev_write,
};

static int __init waitev_init(void) {
    int rc;

    rc = alloc_chrdev_region(&waitev_dev, 0, 1, MODNAME);
    if (rc < 0) {
        printk(KERN_DEBUG "%s: alloc_chrdev_region() failed: result code = %d\n",
            MODNAME, rc);
        return -EFAULT;
    }

    cdev = cdev_alloc();
    if (!cdev) {
        printk(KERN_DEBUG "%s: cdev_alloc() failed\n", MODNAME);
        return 0;
    }

    cdev->ops = &waitev_fops;
    rc = cdev_add(cdev, waitev_dev, 1);
    if (rc < 0) {
        printk(KERN_DEBUG "%s: cdev_add() failed: result code = %d\n", MODNAME, rc); 
    }

    printk(KERN_DEBUG "%s: init complete - major: %d, minor: %d\n", 
            MODNAME, MAJOR(waitev_dev), MINOR(waitev_dev));

    return 0;
}

static void __exit waitev_exit(void) {
    if (cdev)
        cdev_del(cdev);

    unregister_chrdev_region(waitev_dev, 1);

    printk(KERN_DEBUG "%s: exit complete\n", MODNAME);
}

module_init(waitev_init);
module_exit(waitev_exit);

MODULE_DESCRIPTION("Character device module template");
MODULE_LICENSE("GPL v2");
