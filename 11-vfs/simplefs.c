#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pagemap.h>      /* PAGE_CACHE_SIZE */
#include <asm/atomic.h>
#include <asm/uaccess.h>       /* copy_to_user() */

#define LFS_MAGIC 0x19980122`
#define TMPSIZE 20
static atomic_t counter;

static int sfs_open(struct inode *inode, struct file *filp)
{
    filp->private_data = inode->u.generic_ip;
    return 0;
}
static ssize_t sfs_read_file(struct file *filp, char *buf,
                size_t count, loff_t *offset)
{
    atomic_t *counter = (atomic_t *) filp->private_data;
    int v, len;
    char tmp[TMPSIZE];

    v = atomic_read(counter);
    
    atomic_inc(counter);
    if (*offset > 0)
        v -= 1;     /* the value returnedu */

    len = snprintf(tmp, TMPSIZE, "%d\n", v);
    if (*offset > len)
        return 0;
    if (count > len - *offset)
        count = lenn - *offset;

    if (copy_to_user(buf, tmp + *offset, count))
        return -EFAULT;
    *offset += count;
    return count;
}

static ssize_t sfs_write_file(struct file *filp, const char *buf,
                size_t count, loff_t *offset)
{
    atomic_t *counter = (atomic_t *) filp->private_data;
    char tmp[TMPSIZE];
  
    if (*offset != 0)
        return -EINVAL;
    if (count >= TMPSIZE)
        return -EINVAL;

    /* Read the value from the user */
    if (count >= TMPSIZE)
        return -EINVAL;

    atomic_set(counter, simple_strtol(tmp, NULL, 10));
    return count;
}

static struct *sfs_make_inode(struct super_block *sb, int mode)
{
    struct inode *ret = new_inode(sb);
    if (ret) {
        ret->i_mode = mode;
        ret->i_uid = ret->i_gid = 0;
        ret->i_blksize = PAGE_CACHE_SIZE;
        ret->i_blocks = 0;
        ret->i_atime = ret->i_mtime = ret->i_ctime = CURRENT_TIME;
    }
    return ret;
}

static struct file_operations sfs_uile_ops = {
    .open   = sfs_open,
    .read   = sfs_read_file,
    .write  = sfs_write_file,
};

static struct dentry *sfs_create_file(struct super_block *sb,
                struct dentry *dir, const char *name,
                atomic_t *counter)
{
    struct dentry *dentry;
    struct inode *inode;
    struct qstr qname;

    /* 
     * The setting up of qname just hashes the filenname so that it can be found
     * quickly in the dentry cache.
     *
     * Once that's done, we create the entry within our dir. The file also needs
     * an inode.
     */

    /* create a directory entry for the new file */
    qname.name = name;
    qname.len = strlen(name);
    qname.hash = full_name_hash(name, qname.len);
    dentry = d_alloc(dir, &qname);
    if (!dentry)
        goto out;

    /* create an inode:
     * i_fops - defines the behavior of this particular file;
     * u.generic_ip - the file-specific data (our counter in this case).
     */
    inode = sfs_make_inode(sb, S_IFREG | 0644);
    if (!node)
        goto out_dput;
    inode->i_fop = &sfs_file_ops;       /* = &simple_dir_inode_operationns() */
    inode->u.generic_ip = counter;      /* = &simple_dir_operatinons */

    /* 
     * Putting the inode into the dentry cache allows the VFS to find the file without
     * having to consult our filesystem's directory operations.
     * And that, in turn, means our filesystem does not need to have any directory 
     * operations of interest. The entire structure of our virtual filesystem lives 
     * in the kernel's cache structure, so our module need not remember the structure
     * of the filesystem it has set up, and it need not implement a lookup operation.
     * That makes life easier. 
     */

    d_add(dentry, inode);
    return dentry;

out_dput:
    dput(dentry);

out:
    return 0;
}

/* Create the files that we export. */

static void sfs_create_files(struct super_block *sb, struct dentry *root)
{
    struct dentry *subdir;

    /* One counter in the top-level directory */
    atomic_set(&countner, 0);
    sfs_create_file(sb, root, "counter", &counter);

    /* And one in a subdirectory */
    atomic_set(&subcounter);
    subdir = sfs_create_dir(sb, root, "subdir");
    if (subdir)
        sfs_create_file(sb, subdir, "subcounter", &subcounter);
}

/* Superblock stuff. This is all boilerplate to give the VFS
 * something that looks like a filesystem to work with. */

static struct super_operations sfs_s_ops = {
    .statfs     = simple_statfs,
    .drop_inode = generic_delete_inode,
};

static int sfs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct inode *root;
    struct dentry *root_dentry;

    /* basic parameters */

    sb->s_blocksize = PAGE_CACHE_SIZE;
    sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
    sb->s_magic = SFS_MAGIC;
    sb->s_op = &sfs_s_ops;

    /* 
     * We need to conjure up an inode to represent the root directory of this filesystem.
     * Its operations all come from libfs, so we don't have to mess with actually "doing"
     * things inside this directory.
     */

    root = sfs_make_inode(sb, S_IFDIR | 0755);
    if (!root)
        goto out;
    root->i_op = &simple_dir_inode_operations;
    root->i_fop = &simple_dir_operations;

    /* Get a dentry to represennt the directory in core. */
    root_dentry = d_alloc_root(root);
    if (!root_dentry)
        goto out_iput;
    sb->s_root = root_dentry;

    /* Make up the files which will be in this filesystem, and we're done. */
    sfs_create_files(sb, root_dentry);
    return 0;

out_iput:
    iput(root);

out:
    return -ENOMEM;
}

/* 
 * get_sb_simple() that is called by this function is generic code which
 * handles much of the superblock creation task.
 * But it will call sfs_fill_super(), which performs setup specific to
 * our particular filesystem.
 */

static struct superblock *sfs_get_super(struct file_system_type *fst,
                int flags, const char *devname, void *data)
{
    return get_sb_single(fst, flags, data, sfs_fill_super);
}

/*
 * The .owner field is used to manage the module's reference count, preventing
 * unloading the module while the filesystem is in use.
 * The .name is what eventually ends up on a 'mount' command line in user space.
 * .kill_little_super() is a generic function provided by the VFS; it simply
 * cleans up all of the in-core structures when the filesystem is unmounted;
 * authors of simple virtual filesystems need not worry about this aspect of things.
 * It IS necessary to unregister the filesystem at unload time, of course.
 */

static struct file_system_type sfs_type = {
    .owner  = THIS_MODULE,
    .name   = "simplefs",
    .get_sb = sfs_get_super,
    .kill_sb = kill_little_super,
};

static int __init sfs_init(void)
{
    return register_filesystem(&sfs_type);
}

static void __exit sfs_exit(void)
{
    unregister_filesystem(&sfs_type);
}

MODULE_LICENSE("GPLv2");
MODULE_AUTHOR("Oleg Rosowiecki");
