#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqnr.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h> 

#include <asm/atomic.h>

#define DEF_ITEMS 10

int irqnum = 16;
module_param(irqnum, int, 0);

int maxitems = DEF_ITEMS;
module_param(maxitems, int, 0);

atomic_t irq_count = ATOMIC_INIT(0);
atomic_t bh_count = ATOMIC_INIT(0);

struct bh_time {
    struct list_head list;
    struct timeval tv;
};

struct dev_info {
    char *buffer;
    int  length;
    struct tasklet_struct tasklet;
    struct bh_time shots;       /* the list of timestamps -- populated by the botom half tasklet */
    struct timeval irq_time;    /* the most recent timestamp -- populated by the IRQ handler */

    spinlock_t time_lock;       /* protects irq_time */
    spinlock_t list_lock;       /* protects the list of timestamps */
};

static struct dev_info devinfo;

irqreturn_t irq_handler(int irq, void *arg) {
    struct dev_info *dev = (struct dev_info *) arg;

    /* critical section: data shared with bottom half */
    spin_lock(&dev->time_lock);
    do_gettimeofday(&dev->irq_time);
    spin_unlock(&dev->time_lock);
    /* end of critical section */

    atomic_inc(&irq_count);

    /*  schedule the bottom half */
    tasklet_schedule(&dev->tasklet);

    return IRQ_HANDLED;
}

/* called from a bottom half: kmalloc() may not sleep */
struct bh_time *alloc_bh_time(void) {
    return kmalloc(sizeof(struct bh_time), GFP_ATOMIC);
}

void tasklet_bh(unsigned long arg) {
    unsigned long flags;
    struct bh_time *time, *timed;
    struct dev_info *dev = (struct dev_info *) arg;

    if ((time = alloc_bh_time()) == NULL)
        return;         /* skip this bottom half; cannot recover */

    /* critical section: data shared with IRQ handler */
    spin_lock_irqsave(&dev->time_lock, flags);
    memmove(&time->tv, &dev->irq_time, sizeof(dev->irq_time));
    spin_unlock_irqrestore(&dev->time_lock, flags);
    /* end of critical section */

    INIT_LIST_HEAD(&time->list);

    /* critical section: data shared with process context */
    spin_lock(&dev->list_lock);

    /* If count of IRQs exceeds a maximum, delete the least recent entry */
    if (atomic_read(&irq_count) > maxitems) {
        timed = list_entry(dev->shots.list.next, struct bh_time, list);
        list_del(dev->shots.list.next);
        kfree(timed);
    }
    list_add_tail(&time->list, &dev->shots.list);
    spin_unlock(&dev->list_lock);
    /* end of critical section */

    atomic_inc(&bh_count);
}

static void *irqsl_seq_start(struct seq_file *sf, loff_t *pos)
    __acquires(devinfo.list_lock)
{
    struct list_head *lh = devinfo.shots.list.next;
    loff_t p1 = *pos;

    seq_printf(sf, "IRQ count: %d\n", atomic_read(&irq_count));
    seq_printf(sf, "Bottom half count: %d\n", atomic_read(&bh_count));

    spin_lock_bh(&devinfo.list_lock);

    while (lh != &devinfo.shots.list) {
        if (p1-- == 0)
            return lh;
        lh = lh->next;
    }

    return NULL;
}

static void *irqsl_seq_next(struct seq_file *sf, void *v, loff_t *pos)
{
    struct list_head *lh = (struct list_head *) v;
    lh = lh->next;
    (*pos)++;
    return lh == &devinfo.shots.list ? NULL : lh;
}

static void irqsl_seq_stop(struct seq_file *sf, void *v)
    __releases(devinfo.list_lock)
{
    spin_unlock_bh(&devinfo.list_lock);
}

static int irqsl_seq_show(struct seq_file *sf, void *v)
{
    struct list_head *litem = (struct list_head *) v;
    struct bh_time *time = container_of(litem, struct bh_time, list); 
    seq_printf(sf, "Time: %10i.%06i\n", (int) time->tv.tv_sec, (int) time->tv.tv_usec);
    return 0;
}

static struct seq_operations seq_ops = {
    .start      = irqsl_seq_start,
    .next       = irqsl_seq_next,
    .stop       = irqsl_seq_stop,
    .show       = irqsl_seq_show,
};

static int irqsl_seq_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &seq_ops);
}

static struct file_operations proc_fops = {
    .open       = irqsl_seq_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = seq_release,
};

static struct proc_dir_entry *proc_entry;

int __init irqsl_init(void) {
    int rc;

    /*  initialize list head */
    INIT_LIST_HEAD(&devinfo.shots.list);

    /* initialize spinlocks */
    spin_lock_init(&devinfo.time_lock);
    spin_lock_init(&devinfo.list_lock);

    /* initialize the tasklet that will be the bottom half for our IRQ handler */
    tasklet_init(&devinfo.tasklet, tasklet_bh, (unsigned long) &devinfo);

    rc = request_irq(irqnum,
        irq_handler,
        IRQF_SHARED,
        "irqslock",
        &devinfo);
    if (rc < 0) {
        kfree(devinfo.buffer);     /*  free memory before returning */
        pr_debug("Cannot register for irq number %d - reason: %d\n", irqnum, rc);
        return rc;
    }

    proc_entry = proc_create("irqslock", 0666, NULL, &proc_fops);     
    return 0;
}

void __exit irqsl_exit(void) {
    struct bh_time *pos, *next;

    remove_proc_entry("irqslock", NULL);

    /* since we share the IRQ number, we need to supply the same 'cookie' as during handler registration */
    free_irq(irqnum, &devinfo);

    tasklet_kill(&devinfo.tasklet);

    /* the list is no longer shared: safe to free the list items  */
    list_for_each_entry_safe(pos, next, &devinfo.shots.list, list) {
        list_del(&pos->list);   /* tidy up as usual, though it's redundant here */
        kfree(pos);
    }
}

module_init(irqsl_init);
module_exit(irqsl_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Oleg Rosowiecki");
MODULE_DESCRIPTION("spinlock strategies for IRQ handler/bottom half/process context");
