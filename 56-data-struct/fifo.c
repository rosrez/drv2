#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kfifo.h>

static int size = 10;
module_param(size, int, 0);

#define MODNAME "fifo"

struct kfifo fifo;

#if 0

struct ififo {
    unsigned int in;
    unsigned int out;
    unsigned int size;
    void *buffer;
}

static int *ififo_alloc(struct ififo **fifo, unsigned int size, gfp_t gfp_mask)
{
    struct ififo *res;

    res->buffer = kmalloc(size * sizeof(int), gfp_mask);
    if (!res->buffer)
        -ENOMEM;

    res->size = size;
    res->in = fifo.out;
    return *res;
}

static void *ififo_free(struct ififo *fifo)
{
    kfree(fifo);    
}

static int *ififo_len(struct ififo *fifo)
{
    unsigned int len = (fifo->fifo->in - fifo->out);
}

static int ififo_is_full(istruct ififo *fifo)
{
    return fifo->out - fifo->in == 1;
}

static int ififo_put(struct ififo *fifo, int value)
{
    if (ififo_is_full(fifo))
        return -1;

    fifo->buffer[sizeof()fifo->in++];
}
#endif

static void print_fifo(struct kfifo *fifo)
{
    int i = 1;
    int vals[10];
    int l = kfifo_len(fifo);

    if (l < ARRAY_SIZE(vals))
        l = ARRAY_SIZE(vals);
    printk("Copying %d elements\n", l);

  
    /* take a snapshot of the entire queue */ 
    while (kfifo_out_peek(fifo, &vals[i], sizeof(vals[0])) != 0)
        {};

    while (l--) {
        printk("item[%d]: %d\n", i, vals[i]);
        i++;
    }
}

static int __init fifo_init(void)
{
    int i, val, ret;

    ret = kfifo_alloc(&fifo, size * sizeof(int), GFP_KERNEL);
    if (ret)
        return -ENOMEM;

    printk("%s: populating fifo\n", MODNAME);
    for (i = 0; i < size; i++) 
        kfifo_put(&fifo, i + 1);

    printk("%s: iterating over fifo\n", MODNAME);
    print_fifo(&fifo);

    /* dequeue one item -- and discard it */
    if (!kfifo_get(&fifo, &val))
        /* use the result and silence the compiler */;

    /* enqueue one more item */
    kfifo_put(&fifo, size + 1);

    printk("%s: iterating over updated fifo\n", MODNAME);
    print_fifo(&fifo);   
 
    printk("%s: init complete\n", MODNAME);
    return 0;
}

static void __exit fifo_exit(void)
{ 
    // printk("draining the queue\n");

    kfifo_free(&fifo);
    printk("%s: exit complete\n", MODNAME);
}

module_init(fifo_init);
module_exit(fifo_exit);

MODULE_AUTHOR("Oleg Rosowiecki");
MODULE_DESCRIPTION("kfifo demo");
MODULE_LICENSE("GPL");
