#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/hardirq.h>
#include <linux/sched.h>

/* frequency in nanoseconds */
static int ns = 1000;
module_param(ns, int, 0);

/* number of times to fire */
static int shots = 5;
module_param(shots, int, 0);

#define MODNAME "hrtimer"

struct modinfo {
    int count;
    ktime_t period;
    unsigned long prev_jiffies;
    ktime_t prev_ktime;
    struct hrtimer timer;
};

static struct modinfo modinfo;

/*
 * The return value of the high-res timer function indicates whether
 * to fire the timer again:
 *      HRTIMER_RESTART tells the kernel to restart the timer
 *      HRTIMER_NORESTART indicates that the timer should no longer fire
 */

static enum hrtimer_restart hrt_timerfunc(struct hrtimer *timer)
{
    unsigned long j;
    ktime_t kt;

    struct modinfo *info = container_of(timer, struct modinfo, timer);
    info->count++;

    /* get current time: ktime_t version with nanosecond precision & jiffies */
    kt = timer->base->get_time();
    j = jiffies;

    /* print out statistics */
    printk("=== hrtimer: shot %2d ===\n", info->count);
    printk("current ktime (ns): %lld\n", ktime_to_ns(kt));
    printk("ns elapsed since last shot: %lld\n", ktime_to_ns(kt) - ktime_to_ns(info->prev_ktime));

    printk("current jiffies: %lu\n", j);
    printk("jiffies elapsed since last shot: %lu\n", j - info->prev_jiffies);
    printk("in_interrupt: %d, cpu: %d\n", in_interrupt() ? 1 : 0, smp_processor_id());
    printk("PID: %d, command: %s\n", current->pid, current->comm);

    /* update last timestamps */
    info->prev_ktime = kt;
    info->prev_jiffies = j;

    if (info->count < shots) {
        return HRTIMER_RESTART;
    } else {
       return HRTIMER_NORESTART;
    }
}

static int __init hrt_init(void)
{
    printk("%s: initializing\n", MODNAME);
   
    /* get wall clock (REAL clock) time */
    modinfo.prev_ktime = ktime_get_real();
    /* get current jiffies */
    modinfo.prev_jiffies = jiffies;

    hrtimer_init(&modinfo.timer, CLOCK_REALTIME, HRTIMER_MODE_REL);
    /* ktime_set(sec, ns); */
    modinfo.period = ktime_set(0, ns);
    modinfo.timer.function = hrt_timerfunc;
    modinfo.prev_ktime = modinfo.timer.base->get_time();
    hrtimer_start(&modinfo.timer, modinfo.period, HRTIMER_MODE_REL); 

    return 0;
}

static void __exit hrt_exit(void)
{
    hrtimer_cancel(&modinfo.timer);
    printk("%s: exiting\n", MODNAME);
}

module_init(hrt_init);
module_exit(hrt_exit);

MODULE_AUTHOR("Oleg Rosowiecki");
MODULE_DESCRIPTION("HR timer true resolution test");
MODULE_LICENSE("GPL");
