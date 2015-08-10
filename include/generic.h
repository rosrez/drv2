#ifndef __GENERIC_H
#define __GENERIC_H

#define err_return(code, message) do {\
    printk(KERN_DEBUG message ": error code = %d\n", code);\
    return code;\
} while (0)

#define err_out(fmt, arg...) do {\
    printk(KERN_DEBUG fmt "\n", arg);\
} while (0)

#endif  /* __GENERIC_H */
