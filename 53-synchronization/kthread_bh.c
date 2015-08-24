#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/kthread.h>

#define MODNAME "defer_thread"

int period = 5 * HZ;
module_param(period, int, S_IRUGO);

static struct cdev *cdev; 
static dev_t defer_thread_dev;

static DECLARE_WAIT_QUEUE_HEAD(wq);
static atomic_t data_ready = ATOMIC_INIT(0);

static struct task_struct *tsk;
static struct timer_list timer;

static void timer_fn(unsigned long data) {
    printk(KERN_DEBUG "%s: timer function called at %lu\n", MODNAME, jiffies);

    atomic_set(&data_ready, 1);
    wake_up_interruptible(&wq);

    mod_timer(&timer, jiffies + period);
}

static int thread_fn(void *data) {
    do {
        atomic_set(&data_ready, 0);
        wait_event_interruptible(wq, kthread_should_stop() || atomic_read(&data_ready));

        printk(KERN_DEBUG "%s: worker thread woken up at: %lu\n", MODNAME, jiffies);
    } while (!kthread_should_stop());

    return 0;
}

static ssize_t defer_thread_read(struct file *filp, char __user *buf, size_t count, loff_t *pos) {
    return 0;       // EOF
}

static ssize_t defer_thread_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos) {
    return count;   // the writer will think it has written all its data
}

static struct file_operations defer_thread_fops = {
    .owner =    THIS_MODULE,
    .read =     defer_thread_read,
    .write =    defer_thread_write,
};

static int __init defer_thread_init(void) {
    int rc;

    rc = alloc_chrdev_region(&defer_thread_dev, 0, 1, MODNAME);
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

    cdev->ops = &defer_thread_fops;
    rc = cdev_add(cdev, defer_thread_dev, 1);
    if (rc < 0) {
        printk(KERN_DEBUG "%s: cdev_add() failed: result code = %d\n", MODNAME, rc); 
    }

    // init thread
    tsk = kthread_run(thread_fn, NULL, "defer_thread");

    // init timer
    init_timer(&timer);

    timer.function = timer_fn;
    timer.data = 0;
    timer.expires = jiffies + period;

    add_timer(&timer);

    printk(KERN_DEBUG "%s: init complete - major: %d, minor: %d\n", 
            MODNAME, MAJOR(defer_thread_dev), MINOR(defer_thread_dev));

    return 0;
}

static void __exit defer_thread_exit(void) {
    kthread_stop(tsk);

    if (cdev)
        cdev_del(cdev);

    unregister_chrdev_region(defer_thread_dev, 1);

    printk(KERN_DEBUG "%s: exit complete\n", MODNAME);
}

module_init(defer_thread_init);
module_exit(defer_thread_exit);

MODULE_DESCRIPTION("Character device module template");
MODULE_LICENSE("GPL v2");
