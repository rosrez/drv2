#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>

struct stats {
    int count;
};

void *stats_ptr;

static int pid;

static int proc_func(char *buf, char **start, off_t offset,
        int len, int *eof, void *unused)
{
    int cpuno;
    volatile int count; 
    struct stats *st;

    /* set the EOF flag regardless of output */
    *eof = 1;

    /* if we are getting this request twice from the same pid
     * we are most likely being invoked by cat, in which case
     * it's wise to return prematurely, without updating the
     * counter */
    if (current->pid == pid)
        return 0;
    pid = current->pid;

    /* get the address of per-cpu data for current CPU
     * and disable kernel preemption */
    st = get_cpu_ptr(stats_ptr);

    count = st->count;
    printk("percpu: count before increase: %d\n", count);
    count = count + 1;
    printk("percpu: count after increase: %d\n", count);
    st->count = count;
    cpuno = smp_processor_id();

    /* re-enable kernel preemption */
    put_cpu_ptr(stats_ptr);
   
    return sprintf(buf, "Current cpu: %d -- count: %d\n", cpuno, count);
}

static int proc_fun2(char *buf, char **start, off_t offset,
        int len, int *eof, void *unused)
{
    return 0; 
}


static int __init percpu_init(void)
{
    stats_ptr = alloc_percpu(struct stats);
    if (!stats_ptr) {
        printk(KERN_INFO "alloc_percpu() failed -- exiting\n");
        return -ENOMEM;
    }

    create_proc_read_entry("percpu", 0, NULL, proc_func, NULL);
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
