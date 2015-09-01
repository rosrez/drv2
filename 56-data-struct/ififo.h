#ifndef IFIFO_H
#define IFIFO_H

#include <linux/slab.h>

struct ififo {
    unsigned int in;
    unsigned int out;
    unsigned int size;
    int *buffer;
};

int ififo_alloc(struct ififo **fifo, unsigned int size, gfp_t gfp_mask);

void ififo_free(struct ififo *fifo);

int ififo_len(struct ififo *fifo);

int ififo_is_empty(struct ififo *fifo);

int ififo_is_full(struct ififo *fifo);

int ififo_put(struct ififo *fifo, int value);

int ififo_get(struct ififo *fifo, int *value);

int ififo_get_at(struct ififo *fifo, int *value, int pos);

void ififo_copy(struct ififo *fifo, void *buf, int idx, int count);

#endif /* IFIFO_H */
