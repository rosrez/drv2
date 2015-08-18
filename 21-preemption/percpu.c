#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>

struct stats {
    int count;
};

void *stats_ptr;

static void *pcpu_seq_start(struct seq_file *sf, loff_t *pos)
{
    unsigned long cpuno;
    volatile int count; 
    struct stats *st;

    if (*pos >= 1)
        return NULL;

    /* get the address of per-cpu data for current CPU
     * and disable kernel preemption */
    st = get_cpu_ptr(stats_ptr);

    count = st->count;
    pr_info("percpu: count before increase: %d\n", count);
    count = count + 1;
    pr_info("percpu: count after increase: %d\n", count);
    st->count = count;
    cpuno = smp_processor_id();

    /* re-enable kernel preemption */
    put_cpu_ptr(stats_ptr);

    return (void *)((cpuno << 16) & count);
}

static void *pcpu_seq_next(struct seq_file *sf, void *v, loff_t *pos)
{
    return NULL;
}

static void pcpu_seq_stop(struct seq_file *sf, void *v)
{
    /* No-op, may release locks, if required */
}

static int pcpu_seq_show(struct seq_file *sf, void *v)
{
    unsigned long mask = 0xffff;
    unsigned long cpuno, count;

    cpuno = (unsigned long)(v) >> 16;
    count = (unsigned long)(v) & mask;

    seq_printf(sf, "Current cpu: %lu -- count: %lu\n", cpuno, count);
    return 0;
}

static struct seq_operations seq_ops = {
    .start      = pcpu_seq_start,
    .next       = pcpu_seq_next,
    .stop       = pcpu_seq_stop,
    .show       = pcpu_seq_show,
};

static int pcpu_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &seq_ops);
}

static struct file_operations proc_fops = {
    .open       = pcpu_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = seq_release,
};

static struct proc_dir_entry *proc_entry;

static int __init percpu_init(void)
{

    stats_ptr = alloc_percpu(struct stats);
    if (!stats_ptr) {
        printk(KERN_INFO "alloc_percpu() failed -- exiting\n");
        return -ENOMEM;
    }

    proc_entry = proc_create("percpu", 0666, NULL, &proc_fops);
    return 0;
}

static void __exit percpu_exit(void)
{
    remove_proc_entry("percpu", NULL);

    free_percpu(stats_ptr);
}

module_init(percpu_init);
module_exit(percpu_exit);

MODULE_LICENSE("GPL v2");
