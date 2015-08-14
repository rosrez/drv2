#include <linux/module.h>
#include <linux/fs.h>

static struct super_operations sfs_s_ops = {
    .statfs     = simple_statfs,
    .drop_inode = generic_delete_inode,
};

static int sfs_fill_super(struct super_block *sb, void *data, int silent)
{
    sb->s_blocksize = PAGE_CACHE_SIZE;
    sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
    sb->s_magic = SFS_MAGIC;
    sb->s_op = &sfs_s_ops;

    root = sfs_make_inode(sb, S_IFDIR | 0755);
    if (!root)
        goto out;

    root->i_op = &simple_dir_inode_operations;
    root->i_fop = &simple_dir_operations;

    root_dentry = d_alloc_root(root);
    if (!root_dentry)
        goto out_iput;
    sb->s_root = root_dentry;

    return 0;
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

}
