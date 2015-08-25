#include <linux/module.h>
#include <linux/init.h>
#include <linux/init_task.h>        /*  init_task */
#include <linux/mm_types.h>         /*  mm_struct, vma_area_struct etc. */
#include <linux/cdev.h>
#include <linux/seq_file.h> 

#include <asm/uaccess.h>

#define MODNAME "task_struct"

static void print_thread_info(struct seq_file *sf, struct thread_info *thread) {
    seq_printf(sf, "CPU: %d\n", thread->cpu);
    seq_printf(sf, "Address limit: %lx\n", thread->addr_limit.seg);
}

static void print_task_info(struct seq_file *sf, struct task_struct *task) {
    seq_printf(sf, "Command: %s\n", task->comm); 
    seq_printf(sf, "PID: %d\n", task->pid);
}

static void print_mm(struct seq_file *sf, struct mm_struct *mm) {
    seq_printf(sf, "PGD:           %lx\n", mm->pgd->pgd);
    seq_printf(sf, "Environment end:   %lx\n", mm->env_end);
    seq_printf(sf, "Environment:       %lx\n", mm->env_start);
    seq_printf(sf, "Args end:          %lx\n", mm->arg_end);
    seq_printf(sf, "Args start:        %lx\n", mm->arg_start);
    seq_printf(sf, "Stack start:       %lx\n", mm->start_stack);
    seq_printf(sf, "Heap end:          %lx\n", mm->brk);
    seq_printf(sf, "Heap start:        %lx\n", mm->start_brk);
    seq_printf(sf, "Data end:          %lx\n", mm->end_data);
    seq_printf(sf, "Data start:        %lx\n", mm->start_data);
    seq_printf(sf, "Code end:          %lx\n", mm->end_code);
    seq_printf(sf, "Code start:        %lx\n", mm->start_code);
}

static void print_vma(struct seq_file *sf, struct vm_area_struct *vma) {
    char filename[256];
    char flagstr[32];
    int len;
    struct file *file = vma->vm_file;
    
    len = 0;
    if (file != NULL) {
        len = file->f_dentry->d_name.len;
        strncpy(filename, file->f_dentry->d_name.name, len);
    }
    filename[len] = '\0';

    flagstr[0]  = (vma->vm_flags & VM_READ) ? 'r' : '-';
    flagstr[1]  = (vma->vm_flags & VM_WRITE) ? 'w' : '-';
    flagstr[2]  = (vma->vm_flags & VM_EXEC) ? 'x' : '-';
    flagstr[3]  = (vma->vm_flags & VM_SHARED) ? 's' : '-'; 
    flagstr[4]  = (vma->vm_flags & VM_IO) ? 'I' : '-';
    flagstr[5]  = (vma->vm_flags & VM_GROWSUP) ? 'u' : (vma->vm_flags & VM_GROWSDOWN) ? 'd' : '-';
    flagstr[7]  = (vma->vm_flags & VM_LOCKED) ? 'l' : '-';
    flagstr[9]  = (vma->vm_flags & VM_ACCOUNT) ? 'A' : '-';
    flagstr[10] = (vma->vm_flags & VM_HUGETLB) ? 'H' : '-';
    flagstr[11] = (vma->vm_flags & VM_NONLINEAR) ? 'N' : '-';
    flagstr[12] = '\0';

    seq_printf(sf, "%lx - %lx (%lx): %s (%lx) %lx -- %s\n",
        vma->vm_start, vma->vm_end, vma->vm_end - vma->vm_start,
        flagstr, vma->vm_flags, vma->vm_pgoff, filename);
}

static void print_vma_hdr(struct seq_file *sf) {
    seq_printf(sf, "rwx + s (shared)\nS (used for I/O)\nu/d (grows up/down)\n");
    seq_printf(sf, "l (locked)\nI (used for I/O)\nA (accounted VM object)\n");
    seq_printf(sf, "H (huge TLB)\nN (non-linear mapping)\n");
}

static void *ts_seq_start(struct seq_file *sf, loff_t *pos)
{
    struct thread_info *thread = current_thread_info();

    if (*pos != 0)
        return NULL;

    seq_printf(sf, "Current thread info (current): =====\n");
    print_thread_info(sf, thread);
 
    seq_printf(sf, "Current task info (current): =====\n");
    print_task_info(sf, current);

    seq_printf(sf, "Current task memory regions (current->mm): =====\n");
    print_mm(sf, current->mm);
    
    seq_printf(sf, "Current task VM areas: =====\n");
    print_vma_hdr(sf);
    
    return current->mm->mmap;   /* first VMA */
}

static void *ts_seq_next(struct seq_file *sf, void *v, loff_t *pos)
{
    struct vm_area_struct *vma = (struct vm_area_struct *) v;
    vma = vma->vm_next;    /* the list ends with a NULL entry */
    return vma;
}

static void ts_seq_stop(struct seq_file *sf, void *v)
{
    /* No-op, may release locks, if required */
}

static int ts_seq_show(struct seq_file *sf, void *v)
{
    struct vm_area_struct *vma = (struct vm_area_struct *) v;
    print_vma(sf, vma);
    return 0;
}

static struct seq_operations seq_ops = {
    .start      = ts_seq_start,
    .next       = ts_seq_next,
    .stop       = ts_seq_stop,
    .show       = ts_seq_show,
};

static int ts_seq_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &seq_ops);
}

static struct file_operations proc_fops = {
    .open       = ts_seq_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = seq_release,
};

static struct proc_dir_entry *proc_entry;

static int __init ts_init(void) 
{
    proc_entry = proc_create("task_struct", 0666, NULL, &proc_fops);
    pr_info("%s: init complete", MODNAME);
    return 0;
}

static void __exit ts_exit(void) 
{
    remove_proc_entry("vma", NULL);

    pr_info("%s: exit complete\n", MODNAME);
}

module_init(ts_init);
module_exit(ts_exit);

MODULE_AUTHOR("Oleg Rosowiecki");
MODULE_DESCRIPTION("Show task descriptor and individual VMA info contained within");
MODULE_LICENSE("GPL v2");
