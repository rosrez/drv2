/*
 * Demonstrate a trivial filesystem using libfs.  This is the extra
 * simple version without subdirectories.
 *
 * Copyright 2002, 2003 Jonathan Corbet <corbet-AT-lwn.net>
 * This file may be redistributed under the terms of the GNU GPL.
 *
 * Chances are that this code will crash your system, delete your
 * nethack high scores, and set your disk drives on fire.  You have
 * been warned.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>       /* This is where libfs stuff is declared */
#include <asm/atomic.h>
#include <asm/uaccess.h>    /* copy_to_user() */

/*
 * Boilerplate stuff.
 */
MODULE_LICENSE("GPL");

#define LFS_MAGIC 0x19980122

/*
 * Implement an array of counters.
 */

#define NCOUNTERS 4
static atomic_t counters[NCOUNTERS];

/*
 * The operationns on our "files"
 */

/*
 * Open a file. All we have to do here is to copy over a
 * copy of the counter pointer so it's easier to get at.
 */

static int sfs_open(struct inode *inode, struct file *filp)
{
    if (inode->i_ino > NCOUNTERS)
        return -ENODEV;     /* Should never happen. */
    filp->private_data = counters + inode->i_ino - 1;
    return 0;
}

#define TMPSIZE 20

/*
 * Read a file.  Here we increment and read the counter, then pass it
 * back to the caller.  The increment only happens if the read is done
 * at the beginning of the file (offset = 0); otherwise we end up counting
 * by twos.
 */
static ssize_t sfs_read_file(struct file *filp, char *buf, 
                size_t count, loff_t *offset)
{
    int v, len;
    char tmp[TMPSIZE];
    atomic_t *counter = (atomic_t *) filp->private_data;

    /* Encode the value, and figure out how much of it we can pass back */
    v = atomic_read(counter);
    if (*offset > 0)
        v -= 1;     /* the value returnned when offset was zero */
    else
        atomic_inc(counter);

    len = snprintf(tmp, TMPSIZE, "%d\n", v);
    if (*offset > len)
        return 0;
    if (count > len - *offset)
        count = len - *offset;

    /* 
     * Copy it back, increment the offset, and we're done.
     */
    if (copy_to_user(buf, tmp + *offset, count))
        return -EFAULT;
    *offset += count;
    return count;
}

/* 
 * Write a file.
 */

static ssize_t sfs_write_file(struct file *filp, const char *buf,
                size_t count, loff_t *offset)
{
    char tmp[TMPSIZE];
    atomic_t *counter = (atomic_t *) filp->private_data;

    /* Only write from the beginning. */
    if (*offset != 0)
        return -EINVAL;

    /* Read the value from the user */
    if (count >= TMPSIZE)
        return -EINVAL;
    memset(tmp, 0, TMPSIZE);
    if (copy_from_user(tmp, buf, count))
        return -EFAULT;

    /* Store it inn the counter and we are done. */
    atomic_set(counter, simple_strtol(tmp, NULL, 10));
    return count;
}

/*
 * Now we can put together our file operations structure.
 */
static struct file_operations sfs_file_ops = {
    .open = sfs_open,
    .read = sfs_read_file,
    .write = sfs_write_file,
};

/*
 * OK, create the files that we expect.
 */
struct tree_descr OurFiles[] = {
    { NULL, NULL, 0 },  /* Skipped */
    { .name = "counter0",
      .ops = &sfs_file_ops,
      .mode = S_IWUSR|S_IRUGO },
    { .name = "counter1",
      .ops = &sfs_file_ops,
      .mode = S_IWUSR|S_IRUGO },
    { .name = "counter2",
      .ops = &sfs_file_ops,
      .mode = S_IWUSR|S_IRUGO },
    { .name = "counter3",
      .ops = &sfs_file_ops,
      .mode = S_IWUSR|S_IRUGO },
    { "", NULL, 0 }
};

/* 
 * Superblock stuff. This is all boilerplate to give the VFS something
 *  that looks like a filesystem to work with.
 */
static int sfs_fill_super(struct super_block *sb, void *data, int silent)
{
    return simple_fill_super(sb, LFS_MAGIC, OurFiles);
}

/*
 * Stuff to pass in when registering the filesystem.
 */
static int sfs_get_super(struct file_system_type *fst,
                int flags, const char *devname, void *data, struct vfsmount *mount)
{
    return get_sb_single(fst, flags, data, sfs_fill_super, mount);
}

static struct file_system_type sfs_type = {
    .owner      = THIS_MODULE,
    .name       = "verysimplefs",
    .get_sb     = sfs_get_super,
    .kill_sb    = kill_litter_super,
};

/*
 * Get things set up.
 */
static int  __init sfs_init(void)
{
    int i;

    for (i = 0; i < NCOUNTERS; i++)
        atomic_set(counters + i, 0);
    return register_filesystem(&sfs_type);
}

static void __exit sfs_exit(void)
{
    unregister_filesystem(&sfs_type);
}

module_init(sfs_init);
module_exit(sfs_exit);
