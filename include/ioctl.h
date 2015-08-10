#ifndef IOCTL_H
#define IOCTL_H

#ifndef  __KERNEL__
#include <sys/ioctl.h>
#endif /* __kernel */

#define TXT_LEN 32

typedef char text_t[TXT_LEN];

#define IOCTL_MAGIC 'k'

#define IOCTL_REWIND    _IO(IOCTL_MAGIC, 0)
#define IOCTL_NEXT      _IO(IOCTL_MAGIC, 1)
#define IOCTL_HASNEXT   _IOR(IOCTL_MAGIC, 2, int)
#define IOCTL_CLEAR     _IO(IOCTL_MAGIC, 3)
#define IOCTL_ADD       _IOW(IOCTL_MAGIC, 4, text_t)
#define IOCTL_REMOVE    _IOR(IOCTL_MAGIC, 5, int)
#define IOCTL_GET       _IOR(IOCTL_MAGIC, 6, text_t)

#endif /* IOCTL_H */
