#include <linux/slab.h>

#include "ififo.h" 

int ififo_alloc(struct ififo **fifo, unsigned int size, gfp_t gfp_mask)
{
    struct ififo *self;

    self = kzalloc(sizeof(struct ififo) + (size + 1) * sizeof(int), gfp_mask);
    if (!self)
        return -ENOMEM;

    self->size = size + 1;
    self->in = self->out = 0;

    /* the data buffer is located just after the newly allocated struct ififo */
    self->buffer = (int *) (self + 1);
    *fifo = self;
    return 0;
}

void ififo_free(struct ififo *fifo)
{
    kfree(fifo);    
}

int ififo_len(struct ififo *fifo)
{
    int x = 0;
    if (fifo->in < fifo->out)
        x = fifo->size;

    return (x + fifo->in - fifo->out);
}

int ififo_is_empty(struct ififo *fifo)
{
    return fifo->out == fifo->in;
}

int ififo_is_full(struct ififo *fifo)
{
    return ififo_len(fifo) == fifo->size - 1;
}

int ififo_put(struct ififo *fifo, int value)
{
    if (ififo_is_full(fifo))
        return 0;

    fifo->buffer[fifo->in] = value;
    fifo->in = (fifo->in + 1)  % fifo->size;
    return 1;
}

int ififo_get(struct ififo *fifo, int *value)
{
    if (ififo_is_empty(fifo))
        return 0;
    
    if (value)
        *value = fifo->buffer[fifo->out];
    fifo->out = (fifo->out + 1) % fifo->size;
    return 1;
} 

int ififo_get_at(struct ififo *fifo, int *value, int pos)
{
    int tgt;

    if (pos >= ififo_len(fifo))
        return 0;

    tgt = (fifo->out + pos) % fifo->size;

    *value = fifo->buffer[tgt];
    return 1;
} 

void ififo_copy(struct ififo *fifo, int *buf, int idx, int count)
{
    unsigned int tgtb, tgte, len, flen;

    if (ififo_is_empty(fifo))
        return;

    flen = ififo_len(fifo);
    if (idx + count > flen)
        count = flen - idx;

    tgtb = (fifo->out + idx) % fifo->size;
    tgte = (tgtb + count) % fifo->size;

    if (tgtb < tgte) {
        len = tgte - tgtb;
        memcpy(buf, &fifo->buffer[tgtb], len);
    } else {
        len = fifo->size - tgtb;
        printk("copying from %d: %d items\n", tgtb, len);
        memcpy(buf, &fifo->buffer[tgtb], len * sizeof(int));
        buf += len;
        tgtb = 0;
        len = tgte;
        printk("copying from %d: %d items\n", tgtb, len);
        memcpy(buf, &fifo->buffer[tgtb], len * sizeof(int));
    }
}
