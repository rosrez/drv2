#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/fdtable.h>

#define MODNAME "vfsmodule"

#ifndef BUF_SIZE
#define BUF_SIZE ((4) * (PAGE_SIZE))
#endif

#ifndef PATH_SIZE
#define PATH_SIZE 255
#endif

char path_buf[PATH_SIZE + 1];

struct devinfo {
    struct cdev cdev;
    dev_t dev;
    char *kbuf;
    size_t buf_size;
};

static struct devinfo devinfo; 

#if 0

static void close_files(struct files_struct * files)
{
    int i, j;
    struct fdtable *fdt;

    j = 0;

    /*
     *      * It is safe to dereference the fd table without RCU or
     *      * ->file_lock because this is the last reference to the
     *      * files structure.  But use RCU to shut RCU-lockdep up.
     *      */
    rcu_read_lock();
    fdt = files_fdtable(files);
    rcu_read_unlock();
    for (;;) {
        unsigned long set;
        i = j * __NFDBITS;
        if (i >= fdt->max_fds)
            break;
        set = fdt->open_fds->fds_bits[j++];
        while (set) {
            if (set & 1) {
                struct file * file = xchg(&fdt->fd[i], NULL);
                if (file) {
                    filp_close(file, files);
                    cond_resched();
                }
            }
            i++;
            set >>= 1;
        }
    }
}

struct files_struct *get_files_struct(struct task_struct *task)
{
    struct files_struct *files;

    task_lock(task);
    files = task->files;
    if (files)
        atomic_inc(&files->count);
    task_unlock(task);

    return files;
}

void put_files_struct(struct files_struct *files)
{
    struct fdtable *fdt;

    if (atomic_dec_and_test(&files->count)) {
        close_files(files);
        /*
         * Free the fd and fdset arrays if we expanded
         * them.
         * If the fdtable was embedded, pass
         * files for freeing
         * at the end of the RCU
         * grace period. Otherwise,
         * you can free
         * files
         * immediately.
         */
        rcu_read_lock();
        fdt = files_fdtable(files);
        if (fdt != &files->fdtab)
            kmem_cache_free(files_cachep, files);
        free_fdtable(fdt);
        rcu_read_unlock();
    }
}

-------------------

<linux/fs.h>

struct file {
    /*
     *      * fu_list becomes invalid after file_free is called and queued via
     *      * fu_rcuhead for RCU freeing
     *      */
    union {
        struct list_head    fu_list;
        struct rcu_head     fu_rcuhead;
    } f_u;
    struct path     f_path;
#define f_dentry    f_path.dentry
#define f_vfsmnt    f_path.mnt
    const struct file_operations    *f_op;
    spinlock_t      f_lock;  /* f_ep_links, f_flags, no IRQ */
    atomic_long_t       f_count;
    unsigned int        f_flags;
    fmode_t         f_mode;
    loff_t          f_pos;
    struct fown_struct  f_owner;
    const struct cred   *f_cred;
    struct file_ra_state    f_ra;

    u64         f_version;
#ifdef CONFIG_SECURITY
    void            *f_security;
#endif
    /* needed for tty driver, and maybe others */
    void            *private_data;

#ifdef CONFIG_EPOLL
    /* Used by fs/eventpoll.c to link all the hooks to this file */
    struct list_head    f_ep_links;
#endif /* #ifdef CONFIG_EPOLL */
    struct address_space    *f_mapping;
#ifdef CONFIG_DEBUG_WRITECOUNT
    unsigned long f_mnt_write_state;
#endif
};

<linux/fdtable.h>

struct fdtable {
    unsigned int max_fds;
    struct file ** fd;      /* current fd array */
    fd_set *close_on_exec;
    fd_set *open_fds;
    struct rcu_head rcu;
    struct fdtable *next;
};

/*
 *  * Open file table structure
 *  */
struct files_struct {
    /*
     *    * read mostly part
     *    */
    atomic_t count;
    struct fdtable *fdt;
    struct fdtable fdtab;
    /*
     *    * written part on a separate cache line in SMP
     *    */
    spinlock_t file_lock ____cacheline_aligned_in_smp;
    int next_fd;
    struct embedded_fd_set close_on_exec_init;
    struct embedded_fd_set open_fds_init;
    struct file * fd_array[NR_OPEN_DEFAULT];
};


open.c:

void fd_install(unsigned int fd, struct file *file)
{
    struct files_struct *files = current->files;
    struct fdtable *fdt;
    spin_lock(&files->file_lock);
    fdt = files_fdtable(files);
    BUG_ON(fdt->fd[fd] != NULL);
    rcu_assign_pointer(fdt->fd[fd], file);
    spin_unlock(&files->file_lock);
}

#endif

static size_t show_files(void) {
    struct fdtable *fdt;
    struct dentry *dentry;
    struct inode *inode;
    int i, j;
    char *buf = devinfo.kbuf;

   
    spin_lock(&current->files->file_lock);   // rcu_read_lock()
    //rcu_read_lock();

    fdt = files_fdtable(current->files);

    buf += sprintf(buf, "Max fd: %d\n", fdt->max_fds);
    buf += sprintf(buf, "-----\n");

    for (;;) {
        unsigned long set;
        i = j * __NFDBITS;
        if (i >= fdt->max_fds)
            break;
        set = fdt->open_fds->fds_bits[j++];
        while (set) {
            if (set & 1) {
                buf += sprintf(buf, "open fd: %d\n", i); 

                dentry = fdt->fd[i]->f_dentry;
                if (!dentry)
                    continue;

                buf += sprintf(buf, "dentry: %lx\n", (unsigned long) dentry);
                if (dentry->d_name.name)
                    buf += sprintf(buf, "filename: %s\n", dentry->d_name.name);


                inode = dentry->d_inode;
                if (inode) {
                    // mutex_lock(&inode->i_mutex);
                    buf += sprintf(buf, "major: %d, minor: %d\n", imajor(inode), iminor(inode));
                    // buf += sprintf(buf, "inode: %lu\n", inode->i_ino);
                    // mutex_unlock(&inode->i_mutex);
                }

            }
            i++;
            set >>= 1;
        }
    }

    spin_unlock(&current->files->file_lock);     // rcu_read_unlock()
    //rcu_read_unlock();

    // put_files_struct(files);

    return buf - devinfo.kbuf;
}

static ssize_t vfsm_read(struct file *file, char __user *buf, size_t count, loff_t *pos) {
    int nbytes;

    if (*pos >= devinfo.buf_size)
        return 0;                           // Read in entire contents already, return EOF

    if (*pos + count > devinfo.buf_size)
        count = devinfo.buf_size - *pos;    // Read the remaining buffer contents (less than requested)

    nbytes = count - copy_to_user(buf, devinfo.kbuf, count);
    if (!nbytes)
        return -EFAULT;

    *pos += count;
    return count;
}

static ssize_t vfsm_write(struct file *file, const char __user *buf, size_t count, loff_t *pos) {
    int nbytes;

    if (count > PATH_SIZE)
        count = PATH_SIZE;

    nbytes = count - copy_from_user(path_buf, buf, count);
    if (!nbytes)
        return -EFAULT;

    path_buf[nbytes] = '\0';

    devinfo.buf_size = show_files();
    *pos += nbytes;
    return count;
}

static int vfsm_open(struct inode *inode, struct file *file) {
    file->f_pos = 0;        // reset position
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = vfsm_open,
    .read = vfsm_read,
    .write = vfsm_write,
};

static int __init vfsm_init(void)
{
    int rc;

    devinfo.kbuf = kmalloc(BUF_SIZE, GFP_KERNEL);
    if (!devinfo.kbuf) 
        return -ENOMEM;

    rc = alloc_chrdev_region(&devinfo.dev, 0, 1, MODNAME);
    if (rc < 0) {
        kfree(devinfo.kbuf);
        printk(KERN_INFO "%s: cannot allocate char dev region\n", MODNAME);
        return rc;
    }

    cdev_init(&devinfo.cdev, &fops);

    rc = cdev_add(&devinfo.cdev, devinfo.dev, 1);
    if (rc < 0) {
        unregister_chrdev_region(devinfo.dev, 1);
        kfree(devinfo.kbuf);
        printk(KERN_INFO "%s: failed to add char device\n", MODNAME);
        return rc;
    }

    return 0;
}

static void __exit vfsm_exit(void)
{
    unregister_chrdev_region(devinfo.dev, 1);

    kfree(devinfo.kbuf);
}

module_init(vfsm_init);
module_exit(vfsm_exit);

MODULE_LICENSE("GPL v2");
