#ifndef KMACROS_H
#define KMACROS_H

#define err_return(rc, arg...) do { pr_info(arg); return rc; } while (0)

#define err_void(arg...) do { pr_info(arg); return; } while (0)


#endif /* KMACROS_H */
