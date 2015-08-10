#include <linux/module.h>
#include <linux/debugfs.h>

/* The module exercises the various mechanisms provided by debugfs.
 * See Documentation/filesystems/debugfs.txt for detailed info. */

/* To mount the debugfs system, use this command:
 *
 * mount -t debugfs none /sys/kernel/debug
 *
 * The mount point is already provided by a kernel that is compiled with
 * debugfs support. */

#ifndef STR_BUF_SIZE
#define STR_BUF_SIZE 64
#endif

#define DIR_NAME "dfs"

#define err_return(rc, arg...) do { pr_info(arg) ; return rc; } while (0)

struct dentry *dir, *d8, *d16, *d32, *d64;

u8 v8 = 55;
u16 v16 = 666;
u32 v32 = 77777;
u64 v64 = 8888888;

char buf[STR_BUF_SIZE];

static int __init dfs_init(void)
{
    /* 
     * The second parameter is the parent dir:
     * here we locate our directory entry at the topmost level
     */
    dir = debugfs_create_dir(DIR_NAME, NULL);
    if (!dir)
        err_return (-EFAULT, "failed to create dir\n");

    /* The prototype of one of the u... functions to serve as a template:
     
    struct dentry *debugfs_create_u64(const char *name, mode_t mode,
                          struct dentry *parent, u64 *value);
    */

    d8 = debugfs_create_u8("u8", 0600, dir, &v8);
    if (!dir)
        err_return (-EFAULT, "failed to create d8\n");

    d16 = debugfs_create_u16("u16", 0600, dir, &v16);
    if (!dir)
        err_return (-EFAULT, "failed to create d16\n");

    d32 = debugfs_create_u32("u32", 0600, dir, &v32);
    if (!dir)
        err_return (-EFAULT, "failed to create d32\n");

    d64 = debugfs_create_u64("u64", 0600, dir, &v64);
    if (!dir)
        err_return (-EFAULT, "failed to create d64\n");

    return 0;
}

static void __exit dfs_exit(void)
{
    /* We could have removed each entry individually, but there is a way
     * to remove entries recursively, starting from the top directory
     * in a selected hierarchy. So we basically need to keep references
     * only to our top level directories in code. */

    debugfs_remove_recursive(dir);

    /* The function to remove individual entries is:

        void debugfs_remove(struct dentry *dentry);
     */
}

module_init(dfs_init);
module_exit(dfs_exit);

MODULE_AUTHOR("OR");
MODULE_LICENSE("GPL v2");
