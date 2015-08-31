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

static void ififo_copy(struct ififo *fifo, void *buf, int idx, int count)
{
    unsigned int in, len, flen;

    if (ififo_is_empty(fifo))
        return;

    flen = ififo_len(fifo);
    if (idx + count > flen)
        count = ififo_len(fifo) - idx;

    in = (fifo->in + idx) % fifo->size;

    if (in > fifo->out) {
        len = sizeof(int) * (in - flen);
        memcpy(buf, &fifo->buffer[in], len);
    } else {
        len = sizeof(int) * (fifo->size - fifo->out);
        memcpy(buf, &fifo->buffer[in], len);
        in = 0;
        len = sizeof(int) * fifo->in;
        memcpy(buf, &fifo->buffer[in], len);
    }
}

static void print_fifo(struct ififo *fifo)
{
    int i = 1;
    int vals[10];
    int l = ififo_len(fifo);

    if (l < ARRAY_SIZE(vals))
        l = ARRAY_SIZE(vals);
    printk("Copying %d elements\n", l);

    while (l--) {
        printk("item[%d]: %d\n", i, vals[i]);
        i++;
    }
}

static int __init fifo_init(void)
{
    int i, val, ret;

    ret = ififo_alloc(&fifo, size, GFP_KERNEL);
    if (ret)
        return ret;

#if 0
    printk("%s: populating fifo\n", MODNAME);
    for (i = 0; i < size; i++) 
        ififo_put(&fifo, i + 1);

    printk("%s: iterating over fifo\n", MODNAME);
    print_fifo(&fifo);

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
