#ifndef PAGEINFO_IOCTL_H
#define PAGEINFO_IOCTL_H

#ifndef  __KERNEL__
#include <sys/ioctl.h>
#endif /* __kernel */

#define PINFO_MAGIC 'k'

#define PINFO_GET_ADDR    _IO(PINFO_MAGIC, 0) 

#endif /* PAGEINFO_IOCTL_H */
