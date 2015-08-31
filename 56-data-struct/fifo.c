#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>

static int size = 10;
module_param(size, int, 0);

#define MODNAME "fifo"

struct ififo {
    unsigned int in;
    unsigned int out;
    unsigned int size;
    int *buffer;
};

struct ififo *fifo;

static int ififo_alloc(struct ififo **fifo, unsigned int size, gfp_t gfp_mask)
{
    struct ififo *self;

    self = kmalloc(sizeof(struct ififo) + (size + 1) * sizeof(int), gfp_mask);
    if (!self->buffer)
        return -ENOMEM;

    self->size = size + 1;
    self->in = self->out = 0;

    /* the data buffer is located just after the newly allocated struct ififo */
    self->buffer = (int *) (self + 1);
    *fifo = self;
    return 0;
}

static void ififo_free(struct ififo *fifo)
{
    kfree(fifo);    
}

static int ififo_len(struct ififo *fifo)
{
    int x = 0;
    if (fifo->in < fifo->out)
        x = fifo->size;

    return (x + fifo->in - fifo->out);
}

static int ififo_is_empty(struct ififo *fifo)
{
    return fifo->out == fifo->in;
}

static int ififo_is_full(struct ififo *fifo)
{
    int x = 0;
    if (fifo->in < fifo->out) 
        x = fifo->size;

    return x + fifo->out - fifo->in == 1;
}

static int ififo_put(struct ififo *fifo, int value)
{
    if (ififo_is_full(fifo))
        return 0;

    fifo->buffer[fifo->in] = value;
    fifo->in = (fifo->in + 1)  % fifo->size;
    return 1;
}

static int ififo_get(struct ififo *fifo, int *value)
{
    if (ififo_is_empty(fifo))
        return 0;

    *value = fifo->buffer[fifo->out];
    fifo->out = (fifo->out + 1) % fifo->size;
    return 1;
} 

static int ififo_get_at(struct ififo *fifo, int *value, int pos)
{
    int tgt;

    if (pos >= ififo_len(fifo))
        return 0;

    tgt = (fifo->out + pos) % fifo->size;

    *value = fifo->buffer[fifo->out];
    fifo->out = (fifo->out + 1) % fifo->size;
    return 1;
} 

static void ififo_copy(struct ififo *fifo, void *buf, int idx, int count)
{
    unsigned int tgt, len, flen;

    if (ififo_is_empty(fifo))
        return;

    flen = ififo_len(fifo);
    if (idx + count > flen)
        count = ififo_len(fifo) - idx;

    tgt = (fifo->out + idx) % fifo->size;

    if (tgt > fifo->out) {
        len = sizeof(int) * (tgt - flen);
        memcpy(buf, &fifo->buffer[tgt], len);
    } else {
        len = sizeof(int) * (fifo->size - fifo->out);
        memcpy(buf, &fifo->buffer[tgt], len);
        tgt = 0;
        len = sizeof(int) * fifo->in;
        memcpy(buf, &fifo->buffer[tgt], len);
    }
}

static void print_fifo(struct ififo *fifo)
{
    int i = 0;
    int val;

    while (ififo_get_at(fifo, &val, i)) {
        printk("item[%d] = %d\n", i++, val);
    }
}

static int __init fifo_init(void)
{
    int i, val, ret;

    ret = ififo_alloc(&fifo, size, GFP_KERNEL);
    if (ret)
        return ret;

    printk("%s: populating fifo\n", MODNAME);
    for (i = 0; ififo_put(fifo, i + 1); i++);

    printk("%s: iterating over fifo\n", MODNAME);
    print_fifo(fifo);

#if 0
    /* dequeue one item -- and discard it */
    if (!ififo_get(&fifo, &val))
        /* use the result and silence the compiler */;
#endif

    /* enqueue one more item */
    ififo_put(fifo, size + 1);

#if 0
    printk("%s: iterating over updated fifo\n", MODNAME);
    print_fifo(&fifo);   
#endif

    printk("printing -- draining the queue\n");
    i = 1;
    while (ififo_get(fifo, &val)) {
        printk("item[%d] = %d\n", i++, val);
    }
    printk("queue size now is %d\n", ififo_len(fifo));
 
    printk("%s: init complete\n", MODNAME);
    return 0;
}

static void __exit fifo_exit(void)
{ 
    
    ififo_free(fifo);
    printk("%s: exit complete\n", MODNAME);
}

module_init(fifo_init);
module_exit(fifo_exit);

MODULE_AUTHOR("Oleg Rosowiecki");
MODULE_DESCRIPTION("ififo demo");
MODULE_LICENSE("GPL");
