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

#include <asm/atomic.h>

#define MAX_TIMES 10

MODULE_LICENSE("Dual BSD/GPL");

int irq_num = 16;

module_param(irq_num, int, 0);

atomic_t irq_basic_irq_count = ATOMIC_INIT(0);
atomic_t irq_basic_bh_count = ATOMIC_INIT(0);

struct bh_time {
    struct list_head list;
    struct timeval tv;
};

struct irq_dev_info {
    char *buffer;
    int  length;
    struct tasklet_struct tasklet;
    struct bh_time times;
    struct timeval irq_time;

    spinlock_t time_lock;
    spinlock_t list_lock;
};

struct irq_dev_info devinfo;

irqreturn_t irq_handler(int irq, void *dev_id) {
    struct irq_dev_info *dev = (struct irq_dev_info *) dev_id;

    spin_lock(&dev->time_lock);
    pr_info("IRQ: Holding time lock");
    do_gettimeofday(&dev->irq_time);
    spin_unlock(&dev->time_lock);
    pr_info("IRQ: Released time lock");

    atomic_inc(&irq_basic_irq_count);

    // schedule the bottom half
    tasklet_schedule(&devinfo.tasklet);

    return IRQ_HANDLED;
}

struct bh_time *alloc_bh_time(void) {
    return kmalloc(sizeof(struct bh_time), GFP_ATOMIC);
}

void tasklet_bh(unsigned long arg) {
    unsigned long flags;
    struct bh_time *time, *timed;
    struct irq_dev_info *dev = (struct irq_dev_info *) arg;

    time = alloc_bh_time();
    if (time == NULL) {
        pr_debug("alloc_bh_time=() failed");
        return;     // skip this bottom half; cannot recover
    }

    spin_lock_irqsave(&dev->time_lock, flags);
    pr_debug("BH: holding time lock -- adding another time entry");
    memmove(&time->tv, &dev->irq_time, sizeof(dev->irq_time));
    spin_unlock_irqrestore(&dev->time_lock, flags);
    pr_debug("BH: released time lock");

    INIT_LIST_HEAD(&time->list);
    spin_lock(&dev->list_lock);
    pr_debug("BH: holding list lock -- adding another time entry");

    // if count of IRQs exceeds a maximum, delete the first entry
    // this means that our list is essentially a queue of a fixed size
    if (atomic_read(&irq_basic_irq_count) > MAX_TIMES) {
        timed = list_entry(dev->times.list.next, struct bh_time, list);
        list_del(dev->times.list.next);
        kfree(timed);
    }
    list_add_tail(&time->list, &dev->times.list);
    spin_unlock(&dev->list_lock);
    pr_debug("BH: released list lock");

    atomic_inc(&irq_basic_bh_count);
}

int irq_basic_read_procmem(char *buf, char **start, off_t offset,
        int count, int *eof, void *data) {
    struct bh_time *time;
    char *buf2 = buf;

    memmove(buf2, devinfo.buffer, devinfo.length);
    buf2 += devinfo.length;
    buf2 += sprintf(buf2, "IRQ count: %d\n", atomic_read(&irq_basic_irq_count));
    buf2 += sprintf(buf2, "Bottom half count: %d\n", atomic_read(&irq_basic_bh_count));

    spin_lock_bh(&devinfo.list_lock);
    pr_info("PROC: Holding list lock -- adding entry");
    list_for_each_entry(time, &devinfo.times.list, list) {
        buf2 += sprintf(buf2, "Time: %10i.%06i\n", (int) time->tv.tv_sec, (int) time->tv.tv_usec);
    }
    spin_unlock_bh(&devinfo.list_lock);
    pr_info("PROC: Release list lock");

    *eof = 1;
    return buf2 - buf; 
}

int __init irq_basic_init(void) {
    int result;

    devinfo.buffer = kmalloc(4096, GFP_KERNEL);
    if (!devinfo.buffer) {
        printk(KERN_DEBUG "Cannot allocate memory: kmalloc() failed\n");
        return -ENOMEM;
    }

    // initialize list head
    INIT_LIST_HEAD(&devinfo.times.list);

    // init spinlocks
    spin_lock_init(&devinfo.time_lock);
    spin_lock_init(&devinfo.list_lock);

    // initialize the tasklet that will be the bottom half for our IRQ handler
    tasklet_init(&devinfo.tasklet, tasklet_bh, (unsigned long) &devinfo);

    result = request_irq(irq_num,   // IRQ number
        irq_handler,                // the interrupt handler function
        IRQF_SHARED,                // shared interrupt handler (formerly called SA_SHIRQ)
        "irq_basic",                // a name, which appears in /proc/interrupts
        &devinfo);       // any pointer to module's address space will do; this is used to distinguish between handlers
    if (result < 0) {
        kfree(devinfo.buffer);     // free memory before returning
        printk(KERN_DEBUG "Cannot register for irq number %d - reason: %d\n", irq_num, result);
        return result;
    }

    create_proc_read_entry("irqbasic", 0, NULL, irq_basic_read_procmem, NULL);     
    return 0;
}

void __exit irq_basic_exit(void) {
    struct bh_time *pos, *next;

    remove_proc_entry("irqbasic", NULL);    // parent dir is NULL

    // free the IRQ; as we share the IRQ, we need to indentify ourselves,
    // so that the kernel frees up the resource on behalf of the right module;
    // hence the second parameter (a pointer) is set to device struct
    // to match the call to request_irq (see above)
    free_irq(irq_num, &devinfo);

    // after tasklet_kill() returns we can be sure the tasklet will never be scheduled again
    tasklet_kill(&devinfo.tasklet);

    // safe to free the list items; 
    // now that the IRQ handlers and bottom halves are disabled no one else is using them now
    list_for_each_entry_safe(pos, next, &devinfo.times.list, list) {
        kfree(pos);
    }

    kfree(devinfo.buffer); 
}

module_init(irq_basic_init);
module_exit(irq_basic_exit);
