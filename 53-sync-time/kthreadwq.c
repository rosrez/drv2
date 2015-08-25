#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/kthread.h>

#define MODNAME "kthreadwq"

int period = 5 * HZ;
module_param(period, int, S_IRUGO);

int maxlog = 5;
module_param(maxlog, int, S_IRUGO);

static DECLARE_WAIT_QUEUE_HEAD(wq);
static atomic_t data_ready = ATOMIC_INIT(0);

static struct task_struct *tsk;
static struct timer_list timer;

static int logcnt = 0;

static void timer_fn(unsigned long data) 
{
    atomic_set(&data_ready, 1);
    wake_up_interruptible(&wq);

    mod_timer(&timer, jiffies + period);
}

static int thread_fn(void *data) {

    while (!kthread_should_stop()) {
        wait_event_interruptible(wq, kthread_should_stop() || atomic_read(&data_ready));

        if (kthread_should_stop())
            break;              /* we're being stopped, return immediately */

        if (!atomic_read(&data_ready))
            continue; 

        /* now 'acknowledge' that we've woken up */
        atomic_set(&data_ready, 0);

        if (logcnt++ < maxlog)
            pr_info("%s: worker thread woken up at: %lu\n", MODNAME, jiffies);
    }

    return 0;
}

static int __init kthreadwq_init(void) 
{
    /* initialize and run our worker thread */
    tsk = kthread_run(thread_fn, NULL, "kwq_work");
    if (IS_ERR(tsk))
        return PTR_ERR(tsk);

    /* init timer */
    init_timer(&timer);

    timer.function = timer_fn;
    timer.data = 0;
    timer.expires = jiffies + period;

    add_timer(&timer);

    return 0;
}

static void __exit kthreadwq_exit(void)
{
    del_timer_sync(&timer);

    /* stop the thread, don't care about exit status */
    kthread_stop(tsk);
}

module_init(kthreadwq_init);
module_exit(kthreadwq_exit);

MODULE_AUTHOR("Oleg Rosowiecki");
MODULE_DESCRIPTION("kthread/waitqueue demo");
MODULE_LICENSE("GPL");
