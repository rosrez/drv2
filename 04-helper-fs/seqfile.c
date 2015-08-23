#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define MODNAME "seqfile"

int maxitems = 10;
module_param(maxitems, int, S_IRUGO);

static struct proc_dir_entry *proc_entry;

static void *sfil_seq_start(struct seq_file *sf, loff_t *pos)
{
    if (*pos >= maxitems)
        return NULL;
    
    seq_printf(sf, "In start -- pos: %d\n", (int) *pos);
    return (void *) pos;
}       
    
static void *sfil_seq_next(struct seq_file *sf, void *v, loff_t *pos)
{
    (*pos)++;
    return (*pos < maxitems) ? pos : NULL;
}

static void sfil_seq_stop(struct seq_file *sf, void *v) 
{
    /* This method may be a no-op and/or may release locks, if required */
    
    // int i = *(loff_t *) v;

    //seq_printf(sf, "In stop -- pos: %d\n", i);
}       

static int sfil_seq_show(struct seq_file *sf, void *v)
{   
    int i = *(loff_t *) v;
    seq_printf(sf, "Item %d\n", i);
    return 0;
}

static struct seq_operations seq_ops = {
    .start  = sfil_seq_start,
    .next   = sfil_seq_next,
    .stop   = sfil_seq_stop,
    .show   = sfil_seq_show,
};

static int sfil_seq_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &seq_ops);
}

static struct file_operations fops = {
    .open       = sfil_seq_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = seq_release,
};

static int __init sfil_init(void)
{
    proc_entry = proc_create("seqfile", 0666, NULL, &fops);
    pr_info("%s: init complete", MODNAME);
    return 0;
}

static void __exit sfil_exit(void)
{
    remove_proc_entry("seqfile", NULL);
    pr_info("%s: exit complete\n", MODNAME);
}

module_init(sfil_init);
module_exit(sfil_exit);

MODULE_AUTHOR("Oleg Rosowiecki");
MODULE_DESCRIPTION();
MODULE_LICENSE("GPL v2");
