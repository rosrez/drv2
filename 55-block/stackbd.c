#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h> /* printk() */
#include <linux/fs.h>     /* everything... */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/kthread.h>

#include <trace/events/block.h>

#define DEVNAME "stackbd"
#define DEVNAME_0 "stackbd0"
#define STACKBD_DO_IT 1000

#define STACKBD_BDEV_MODE (FMODE_READ | FMODE_WRITE | FMODE_EXCL)
#define DEBUGGG printk("stackbd: %d\n", __LINE__);
/*
 * We can tweak our hardware sector size, but the kernel talks to us
 * in terms of small sectors, always.
 */
#define KERNEL_SECTOR_SIZE 512


MODULE_LICENSE("Dual BSD/GPL");

static int major_num = 0;
module_param(major_num, int, 0);
static int LOGICAL_BLOCK_SIZE = 512;
module_param(LOGICAL_BLOCK_SIZE, int, 0);

char targetname[256];
module_param_string(target, targetname, sizeof(targetname), 0);

/*
 * The internal representation of our device.
 */
static struct stackbd_t {
    sector_t capacity; /* Sectors */
    struct gendisk *gd;
    spinlock_t lock;
    struct bio_list bio_list;    
    struct task_struct *thread;
    int is_active;
    struct block_device *bdev_raw;
    /* Our request queue */
    struct request_queue *queue;
} stackbd;

static DECLARE_WAIT_QUEUE_HEAD(req_event);

#define trace_block_bio_remap trace_block_remap

static void stackbd_endio(struct bio *cloned_bio, int error)
{
    struct bio *bio = cloned_bio->bi_private;

    //FIXME:  int uptodate = test_bit(BIO_UPTODATE, &cloned_bio->bi_flags);

    pr_info("%s: endio -- size: %u\n", DEVNAME, cloned_bio->bi_size);

    bio_endio(bio, error);

/*
    pr_info("%s: endio -- size: %u -- up-to-date: %d-- error: %d -- in_inter: %lu\n", 
        DEVNAME, cloned_bio->bi_size, uptodate, error, in_interrupt());
*/
    //bio_put(cloned_bio);

#if 0
   pr_info("%s: endio -- size: %u -- up-to-date: %d-- error: %d -- in_inter: %lu -- comm: %s\n", 
        DEVNAME, bio->bi_size, uptodate, error, in_interrupt(), current->comm);
#endif
}

/*
 * Handle an I/O request.
 */
/* FIXME:VER */
/* static void stackbd_make_request(struct request_queue *q, struct bio *bio) */
static int stackbd_make_request(struct request_queue *q, struct bio *bio)
{
    unsigned long flags;
    struct bio *cloned_bio;

    printk("stackbd: make request %-5s block %-12llu #pages %-4hu total-size "
            "%-10u\n", bio_data_dir(bio) == WRITE ? "write" : "read",
            (unsigned long long) bio->bi_sector, bio->bi_vcnt, bio->bi_size);
    printk("%s: bi_end_io = %p", DEVNAME, bio->bi_end_io);

//    printk("<%p> Make request %s %s %s\n", bio,
//           bio->bi_rw & REQ_SYNC ? "SYNC" : "",
//           bio->bi_rw & REQ_FLUSH ? "FLUSH" : "",
//           bio->bi_rw & REQ_NOIDLE ? "NOIDLE" : "");
//
    spin_lock_irqsave(&stackbd.lock, flags);
    if (!stackbd.bdev_raw)
    {
        printk("stackbd: Request before bdev_raw is ready, aborting\n");
        goto abort;
    }
    if (!stackbd.is_active)
    {
        printk("stackbd: Device not active yet, aborting\n");
        goto abort;
    }
    
    spin_unlock_irqrestore(&stackbd.lock, flags);
    
    cloned_bio = bio_clone(bio, GFP_NOIO);
    cloned_bio->bi_bdev = stackbd.bdev_raw;
    cloned_bio->bi_end_io = stackbd_endio;
    cloned_bio->bi_private = bio;

#if 0
    cloned_bio = bio;
    cloned_bio->bi_bdev = stackbd.bdev_raw;
    cloned_bio->bi_private = cloned_bio->bi_end_io;
    cloned_bio->bi_end_io = stackbd_endio;
    generic_make_request(cloned_bio);
#endif

    generic_make_request(cloned_bio);
    /* FIXME:VER return; */
    return 0;

abort:
    spin_unlock_irq(&stackbd.lock);
    printk("<%p> Abort request\n\n", bio);
    bio_io_error(bio);
    
    /*FIXME:VER return; */
    return 0;
}

static struct block_device *stackbd_bdev_open(char dev_path[])
{
    /* Open underlying device */
    struct block_device *bdev_raw = lookup_bdev(dev_path);
    printk("Opened %s\n", dev_path);

    if (IS_ERR(bdev_raw))
    {
        printk("stackbd: error opening raw device <%lu>\n", PTR_ERR(bdev_raw));
        return NULL;
    }

    if (!bdget(bdev_raw->bd_dev))
    {
        printk("stackbd: error bdget()\n");
        return NULL;
    }


    /* FIXME:VER */
    /*    if (blkdev_get(bdev_raw, STACKBD_BDEV_MODE, &stackbd))*/
    if (blkdev_get(bdev_raw, STACKBD_BDEV_MODE))
    {
        printk("stackbd: error blkdev_get()\n");
        bdput(bdev_raw);
        return NULL;
    }

    if (bd_claim(bdev_raw, &stackbd)) {
        printk("stackbd: error bd_claim()\n");
        bdput(bdev_raw);
        return NULL;
    }

    return bdev_raw;
}

static int stackbd_start(char *dev_path)
{
    unsigned max_sectors;

    if (!(stackbd.bdev_raw = stackbd_bdev_open(dev_path)))
        return -EFAULT;

    /* Set up our internal device */
    stackbd.capacity = get_capacity(stackbd.bdev_raw->bd_disk);
    printk("stackbd: Device real capacity: %llu\n", (unsigned long long) stackbd.capacity);

    set_capacity(stackbd.gd, stackbd.capacity);

    max_sectors = queue_max_hw_sectors(bdev_get_queue(stackbd.bdev_raw));
    blk_queue_max_hw_sectors(stackbd.queue, max_sectors);
    printk("stackbd: Max sectors: %u\n", max_sectors);

    printk("stackbd: done initializing successfully\n");
    stackbd.is_active = 1;

    return 0;
}

static int stackbd_ioctl(struct block_device *bdev, fmode_t mode,
		     unsigned int cmd, unsigned long arg)
{
    char dev_path[80];
	void __user *argp = (void __user *)arg;    

    switch (cmd)
    {
    case STACKBD_DO_IT:
        printk("\n*** DO IT!!!!!!! ***\n\n");

        if (copy_from_user(dev_path, argp, sizeof(dev_path)))
            return -EFAULT;

        return stackbd_start(dev_path);
    default:
        return -ENOTTY;
    }
}

/*
 * The HDIO_GETGEO ioctl is handled in blkdev_ioctl(), which
 * calls this. We need to implement getgeo, since we can't
 * use tools such as fdisk to partition the drive otherwise.
 */
int stackbd_getgeo(struct block_device * block_device, struct hd_geometry * geo)
{
	long size;

	/* We have no real geometry, of course, so make something up. */
	size = stackbd.capacity * (LOGICAL_BLOCK_SIZE / KERNEL_SECTOR_SIZE);
	geo->cylinders = (size & ~0x3f) >> 6;
	geo->heads = 4;
	geo->sectors = 16;
	geo->start = 0;
	return 0;
}

/*
 * The device operations structure.
 */
static struct block_device_operations stackbd_ops = {
		.owner  = THIS_MODULE,
		.getgeo = stackbd_getgeo,
        .ioctl  = stackbd_ioctl,
};

static int __init stackbd_init(void)
{
    if (strcmp(targetname,"") == 0)
        return -EINVAL;

	/* Set up our internal device */
	spin_lock_init(&stackbd.lock);

	/* blk_alloc_queue() instead of blk_init_queue() so it won't set up the
     * queue for requests.
     */
    if (!(stackbd.queue = blk_alloc_queue(GFP_KERNEL)))
    {
        printk("stackbd: alloc_queue failed\n");
        return -EFAULT;
    }

    blk_queue_make_request(stackbd.queue, stackbd_make_request);
	blk_queue_logical_block_size(stackbd.queue, LOGICAL_BLOCK_SIZE);

	/* Get registered */
	if ((major_num = register_blkdev(major_num, DEVNAME)) < 0)
    {
		printk("stackbd: unable to get major number\n");
		goto error_after_alloc_queue;
	}

	/* Gendisk structure */
	if (!(stackbd.gd = alloc_disk(16)))
		goto error_after_redister_blkdev;
	stackbd.gd->major = major_num;
	stackbd.gd->first_minor = 0;
	stackbd.gd->fops = &stackbd_ops;
	stackbd.gd->private_data = &stackbd;
	strcpy(stackbd.gd->disk_name, DEVNAME_0);
	stackbd.gd->queue = stackbd.queue;
	add_disk(stackbd.gd);

    printk("stackbd: init done\n");

	return stackbd_start(targetname);

error_after_redister_blkdev:
	unregister_blkdev(major_num, DEVNAME);
error_after_alloc_queue:
    blk_cleanup_queue(stackbd.queue);

	return -EFAULT;
}

static void __exit stackbd_exit(void)
{
    printk("stackbd: exit\n");

    if (stackbd.is_active)
    {
        kthread_stop(stackbd.thread);
        blkdev_put(stackbd.bdev_raw, STACKBD_BDEV_MODE);
        bdput(stackbd. bdev_raw);
    }

	del_gendisk(stackbd.gd);
	put_disk(stackbd.gd);
	unregister_blkdev(major_num, DEVNAME);
	blk_cleanup_queue(stackbd.queue);
}

module_init(stackbd_init);
module_exit(stackbd_exit);
