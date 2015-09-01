#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>

#include "ififo.h"

static int size = 10;
module_param(size, int, 0);

#define MODNAME "fifo"

struct ififo *fifo;

static void print_fifo(struct ififo *fifo)
{
    int i = 0;
    int val;

    while (ififo_get_at(fifo, &val, i)) {
        printk("item[%d] = %d\n", i + 1, val);
        i++;
    }
}

static void print_fifo_copy(struct ififo *fifo)
{
    static int vals[1000];
    int i;
    int l = ififo_len(fifo);

    /* copy the entire fifo to the array */
    ififo_copy(fifo, &vals[0], 0, l);

    for (i = 0; i < l; i++)
        printk("item[%d] = %d\n", i + 1, vals[i]);
}

static void add_one(struct ififo *fifo, int value)
{
    int dummy;

    /* dequeue one item -- and discard it */
    ififo_get(fifo, &dummy);

    /* throw in another item */
    if (!ififo_put(fifo, value)) 
        printk("unsuccessful attempt to insert to fifo after dequeueing\n");
}

static void add_half(struct ififo *fifo, int value)
{
    int i;
    int l = ififo_len(fifo) / 2;
    
    for (i = 0; i < l; i++, value++)
        add_one(fifo, value);
}

static int __init fifo_init(void)
{
    int i, val, ret;

    ret = ififo_alloc(&fifo, size, GFP_KERNEL);
    if (ret)
        return ret;

    printk("%s: populating fifo\n", MODNAME);
    for (i = 0; ififo_put(fifo, i + 1); i++);

    printk("%s: initial fifo state\n", MODNAME);
    print_fifo(fifo);

    printk("%s: COPY -- initial fifo state\n", MODNAME);
    print_fifo(fifo);

    /* try to enqueue one more item */
    if (!ififo_put(fifo, size + 1)) 
        printk("unsuccessful attempt to insert to full fifo\n");

    add_one(fifo, size + 1);
    printk("%s: added 1 item\n", MODNAME);
    print_fifo(fifo);

    /* add another half items */
    add_half(fifo, size + 2);
    printk("%s: added half\n", MODNAME);
    print_fifo(fifo);   

    printk("%s: COPY -- after adding half\n", MODNAME);
    print_fifo_copy(fifo);   

    printk("printing -- draining the queue\n");
    i = 1;
    while (ififo_get(fifo, &val)) 
        printk("item[%d] = %d\n", i++, val);
    
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
