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

#define STACKBD_NAME_0 "blkstat0"
#define STACKBD_NAME "blkstat"
#define STACKBD_DO_IT 5

#define STACKBD_BDEV_MODE (FMODE_READ | FMODE_WRITE | FMODE_EXCL)
#define DEBUGGG printk("bstat: %d\n", __LINE__);
/*
 * We can tweak our hardware sector size, but the kernel talks to us
 * in terms of small sectors, always.
 */
#define KERNEL_SECTOR_SIZE 512

static int major_num = 0;
module_param(major_num, int, 0);
static int LOGICAL_BLOCK_SIZE = 512;
module_param(LOGICAL_BLOCK_SIZE, int, 0);

static struct bstat_t {
    struct gendisk *gendisk;
    struct block_device *tdev;
    struct request_queue *queue;

    sector_t capacity;     
    spinlock_t lock;
    struct bio_list bio_list;    
    struct task_struct *thread;
    int is_active;
} bstat;

static DECLARE_WAIT_QUEUE_HEAD(req_event);

static void bstat_io_fn(struct bio *bio)
{
//    printk("stackdb: Mapping sector: %llu -> %llu, dev: %s -> %s\n",
//            bio->bi_sector,
//            lba != EMPTY_REAL_LBA ? lba : bio->bi_sector,
//            bio->bi_bdev->bd_disk->disk_name,
//            tdev->bd_disk->disk_name);
//
//    if (lba != EMPTY_REAL_LBA)
//        bio->bi_sector = lba;
    bio->bi_bdev = bstat.tdev;

    trace_block_bio_remap(bdev_get_queue(bstat.tdev), bio,
            bio->bi_bdev->bd_dev, bio->bi_iter.bi_sector);

    /* No need to call bio_endio() */
    generic_make_request(bio);
}

static int bstat_threadfn(void *data)
{
    struct bio *bio;

    set_user_nice(current, -20);

    while (!kthread_should_stop())
    {
        /* wake_up() is after adding bio to list. No need for condition */ 
        wait_event_interruptible(req_event, kthread_should_stop() ||
                !bio_list_empty(&bstat.bio_list));

        spin_lock_irq(&bstat.lock);
        if (bio_list_empty(&bstat.bio_list))
        {
            spin_unlock_irq(&bstat.lock);
            continue;
        }

        bio = bio_list_pop(&bstat.bio_list);
        spin_unlock_irq(&bstat.lock);

        bstat_io_fn(bio);
    }

    return 0;
}

/*
 * Handle an I/O request.
 */
static void bstat_make_request(struct request_queue *q, struct bio *bio) 
{
    printk("bstat: make request %-5s block %-12llu #pages %-4hu total-size "
            "%-10u\n", bio_data_dir(bio) == WRITE ? "write" : "read",
            (unsigned long long) bio->bi_iter.bi_sector, bio->bi_vcnt, bio->bi_iter.bi_size);

//    printk("<%p> Make request %s %s %s\n", bio,
//           bio->bi_rw & REQ_SYNC ? "SYNC" : "",
//           bio->bi_rw & REQ_FLUSH ? "FLUSH" : "",
//           bio->bi_rw & REQ_NOIDLE ? "NOIDLE" : "");
//
    spin_lock_irq(&bstat.lock);
    if (!bstat.tdev)
    {
        printk("bstat: Request before tdev is ready, aborting\n");
        goto abort;
    }
    if (!bstat.is_active)
    {
        printk("bstat: Device not active yet, aborting\n");
        goto abort;
    }
    bio_list_add(&bstat.bio_list, bio);
    wake_up(&req_event);
    spin_unlock_irq(&bstat.lock);

    return;

abort:
    spin_unlock_irq(&bstat.lock);
    printk("<%p> Abort request\n\n", bio);
    bio_io_error(bio);
    
    return;
}

static struct block_device *bstat_bdev_open(char dev_path[])
{
    /* Open underlying device */
    struct block_device *tdev = lookup_bdev(dev_path);
    if (IS_ERR(tdev))
    {
        printk("bstat: error opening raw device <%lu>\n", PTR_ERR(tdev));
        return NULL;
    }

    if (!bdget(tdev->bd_dev))
    {
        printk("bstat: error bdget()\n");
        return NULL;
    }

    if (blkdev_get(tdev, STACKBD_BDEV_MODE, &bstat))
    {
        printk("bstat: error blkdev_get()\n");
        bdput(tdev);
        return NULL;
    }

    printk("Opened %s\n", dev_path);

    return tdev;
}

static int bstat_start(char dev_path[])
{
    unsigned max_sectors;

    if (!(bstat.tdev = bstat_bdev_open(dev_path)))
        return -EFAULT;

    /* Set up our internal device */
    bstat.capacity = get_capacity(bstat.tdev->bd_disk);
    printk("bstat: Device real capacity: %llu\n", (unsigned long long) bstat.capacity);

    set_capacity(bstat.gendisk, bstat.capacity);

    max_sectors = queue_max_hw_sectors(bdev_get_queue(bstat.tdev));
    blk_queue_max_hw_sectors(bstat.queue, max_sectors);
    printk("bstat: Max sectors: %u\n", max_sectors);

    bstat.thread = kthread_create(bstat_threadfn, NULL,
           bstat.gendisk->disk_name);
    if (IS_ERR(bstat.thread))
    {
        printk("bstat: error kthread_create <%lu>\n",
               PTR_ERR(bstat.thread));
        goto error_after_bdev;
    }

    printk("bstat: done initializing successfully\n");
    bstat.is_active = 1;
    wake_up_process(bstat.thread);

    return 0;

error_after_bdev:
    blkdev_put(bstat.tdev, STACKBD_BDEV_MODE);
    bdput(bstat.tdev);

    return -EFAULT;
}

static int bstat_ioctl(struct block_device *bdev, fmode_t mode,
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

        return bstat_start(dev_path);
    default:
        return -ENOTTY;
    }
}

/*
 * The HDIO_GETGEO ioctl is handled in blkdev_ioctl(), which
 * calls this. We need to implement getgeo, since we can't
 * use tools such as fdisk to partition the drive otherwise.
 */
int bstat_getgeo(struct block_device * block_device, struct hd_geometry * geo)
{
	long size;

	/* We have no real geometry, of course, so make something up. */
	size = bstat.capacity * (LOGICAL_BLOCK_SIZE / KERNEL_SECTOR_SIZE);
	geo->cylinders = (size & ~0x3f) >> 6;
	geo->heads = 4;
	geo->sectors = 16;
	geo->start = 0;
	return 0;
}

/*
 * The device operations structure.
 */
static struct block_device_operations bstat_ops = {
		.owner  = THIS_MODULE,
		.getgeo = bstat_getgeo,
        .ioctl  = bstat_ioctl,
};

static int __init bstat_init(void)
{
	/* Set up our internal device */
	spin_lock_init(&bstat.lock);

	/* blk_alloc_queue() instead of blk_init_queue() so it won't set up the
     * queue for requests.
     */
    if (!(bstat.queue = blk_alloc_queue(GFP_KERNEL)))
    {
        printk("bstat: alloc_queue failed\n");
        return -EFAULT;
    }

    blk_queue_make_request(bstat.queue, bstat_make_request);
	blk_queue_logical_block_size(bstat.queue, LOGICAL_BLOCK_SIZE);

	/* Get registered */
	if ((major_num = register_blkdev(major_num, STACKBD_NAME)) < 0)
    {
		printk("bstat: unable to get major number\n");
		goto error_after_alloc_queue;
	}

	/* Gendisk structure */
	if (!(bstat.gendisk = alloc_disk(16)))
		goto error_after_redister_blkdev;
	bstat.gendisk->major = major_num;
	bstat.gendisk->first_minor = 0;
	bstat.gendisk->fops = &bstat_ops;
	bstat.gendisk->private_data = &bstat;
	strcpy(bstat.gendisk->disk_name, STACKBD_NAME_0);
	bstat.gendisk->queue = bstat.queue;
	add_disk(bstat.gendisk);

    printk("bstat: init done\n");

	return 0;

error_after_redister_blkdev:
	unregister_blkdev(major_num, STACKBD_NAME);
error_after_alloc_queue:
    blk_cleanup_queue(bstat.queue);

	return -EFAULT;
}

static void __exit bstat_exit(void)
{
    printk("bstat: exit\n");

    if (bstat.is_active)
    {
        kthread_stop(bstat.thread);
        blkdev_put(bstat.tdev, STACKBD_BDEV_MODE);
        bdput(bstat. tdev);
    }

	del_gendisk(bstat.gendisk);
	put_disk(bstat.gendisk);
	unregister_blkdev(major_num, STACKBD_NAME);
	blk_cleanup_queue(bstat.queue);
}

module_init(bstat_init);
module_exit(bstat_exit);

MODULE_AUTHOR("Oleg Rosowiecki");
MODULE_LICENSE("GPL");
