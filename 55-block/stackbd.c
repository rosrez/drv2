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

#define DEVNAME "blkstat"
#define DEVNAME_0 "blkstat0"

#define NR_MINORS 1

#define blkstat_BDEV_MODE (FMODE_READ | FMODE_WRITE | FMODE_EXCL)
#define DEBUGGG printk("blkstat: %d\n", __LINE__);
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

/* placeholder for extra data assigned to bi_private of cloned bios */
struct biostat {
    struct bio *bio;        /* original bio */
    unsigned long time;     /* timestamp of submission of cloned bio */
};

/*
 * The internal representation of our device.
 */
static struct blkstat_t {
    sector_t capacity; /* Sectors */
    struct gendisk *gendisk;
    spinlock_t lock;
    struct bio_list bio_list;    
    struct task_struct *thread;
    int is_active;
    struct block_device *tdev;
    /* Our request queue */
    struct request_queue *queue;
} blkstat;

static struct biostat *alloc_biostat(struct bio *bio)
{
    struct biostat *bs = kmalloc(sizeof(*bs), GFP_NOIO);
    if (!bs)
        return NULL;

    bs->bio = bio;
    bs->time = ktime_to_ns(ktime_get()); /* get the current timestamp */
    return bs;
}

static void free_biostat(struct biostat *bs)
{
    kfree(bs);
}

static void blkstat_endio(struct bio *cloned_bio, int error)
{
    struct biostat *bs = cloned_bio->bi_private;
    struct bio *bio = bs->bio;
    
    int uptodate = test_bit(BIO_UPTODATE, &cloned_bio->bi_flags);

    unsigned long now = ktime_to_ns(ktime_get());
    unsigned long nselapsed = now - bs->time;

    pr_info("%s: endio -- elapsed: %lu -- size: %u -- up-to-date: %d-- error: %d -- IRQ: %lu -- INTR: %lu\n", 
        DEVNAME, nselapsed, cloned_bio->bi_size, uptodate, error, in_irq(), in_interrupt());

    bio_endio(bio, error);
    free_biostat(bs);
}

/* FIXME:VER */
/* static void blkstat_make_request(struct request_queue *q, struct bio *bio) */
static int blkstat_make_request(struct request_queue *q, struct bio *bio)
{
    struct bio *cloned_bio;

    printk("blkstat: make request %-5s block %-12llu #pages %-4hu total-size "
            "%-10u\n", bio_data_dir(bio) == WRITE ? "write" : "read",
            (unsigned long long) bio->bi_sector, bio->bi_vcnt, bio->bi_size);

    if (!blkstat.tdev)
    {
        printk("blkstat: Request before tdev is ready, aborting\n");
        goto err_bio;
    }
    if (!blkstat.is_active)
    {
        printk("blkstat: Device not active yet, aborting\n");
        goto err_bio;
    }
    
    cloned_bio = bio_clone(bio, GFP_NOIO);
    if (!cloned_bio)
        goto err_bio; 

    /* populate the necessary bio info */

    cloned_bio->bi_private = alloc_biostat(bio);
    if (!cloned_bio->bi_private)
        goto err_free_clone;

    cloned_bio->bi_bdev = blkstat.tdev;
    cloned_bio->bi_end_io = blkstat_endio;

    generic_make_request(cloned_bio);
    /* FIXME:VER return; */
    return 0;

err_free_clone:
    bio_put(cloned_bio);

err_bio:
    bio_io_error(bio);
    return 0;
}

static struct block_device *blkstat_bdev_open(char dev_path[])
{
    /* Open underlying device */
    struct block_device *tdev = lookup_bdev(dev_path);
    printk("Opened %s\n", dev_path);

    if (IS_ERR(tdev))
    {
        printk("blkstat: error opening raw device <%lu>\n", PTR_ERR(tdev));
        return NULL;
    }

    if (!bdget(tdev->bd_dev))
    {
        printk("blkstat: error bdget()\n");
        return NULL;
    }


    /* FIXME:VER */
    /*    if (blkdev_get(tdev, blkstat_BDEV_MODE, &blkstat))*/
    if (blkdev_get(tdev, blkstat_BDEV_MODE))
    {
        printk("blkstat: error blkdev_get()\n");
        bdput(tdev);
        return NULL;
    }

    if (bd_claim(tdev, &blkstat)) {
        printk("blkstat: error bd_claim()\n");
        bdput(tdev);
        return NULL;
    }

    return tdev;
}

static int blkstat_start(char *dev_path)
{
    unsigned max_sectors;

    if (!(blkstat.tdev = blkstat_bdev_open(dev_path)))
        return -EFAULT;

    /* Set up our internal device */
    blkstat.capacity = get_capacity(blkstat.tdev->bd_disk);
    printk("blkstat: Device real capacity: %llu\n", (unsigned long long) blkstat.capacity);

    set_capacity(blkstat.gendisk, blkstat.capacity);

    max_sectors = queue_max_hw_sectors(bdev_get_queue(blkstat.tdev));
    blk_queue_max_hw_sectors(blkstat.queue, max_sectors);
    printk("blkstat: Max sectors: %u\n", max_sectors);

    printk("blkstat: done initializing successfully\n");
    blkstat.is_active = 1;

    return 0;
}

/*
 * The HDIO_GETGEO ioctl is handled in blkdev_ioctl(), which
 * calls this. We need to implement getgeo, since we can't
 * use tools such as fdisk to partition the drive otherwise.
 */
int blkstat_getgeo(struct block_device * block_device, struct hd_geometry * geo)
{
	long size;

	/* We have no real geometry, of course, so make something up. */
	size = blkstat.capacity * (LOGICAL_BLOCK_SIZE / KERNEL_SECTOR_SIZE);
	geo->cylinders = (size & ~0x3f) >> 6;
	geo->heads = 4;
	geo->sectors = 16;
	geo->start = 0;
	return 0;
}

/*
 * The device operations structure.
 */
static struct block_device_operations blkstat_ops = {
		.owner  = THIS_MODULE,
		.getgeo = blkstat_getgeo,
};

static int __init blkstat_init(void)
{
    if (strcmp(targetname,"") == 0)
        return -EINVAL;

	/* Set up our internal device */
	spin_lock_init(&blkstat.lock);

	/* blk_alloc_queue() instead of blk_init_queue() so it won't set up the
     * queue for requests.
     */
    if (!(blkstat.queue = blk_alloc_queue(GFP_KERNEL)))
    {
        printk("blkstat: alloc_queue failed\n");
        return -EFAULT;
    }

    blk_queue_make_request(blkstat.queue, blkstat_make_request);
	blk_queue_logical_block_size(blkstat.queue, LOGICAL_BLOCK_SIZE);

	/* Get registered */
	if ((major_num = register_blkdev(major_num, DEVNAME)) < 0)
    {
		printk("blkstat: unable to get major number\n");
		goto error_after_alloc_queue;
	}

	/* Gendisk structure */
	if (!(blkstat.gendisk = alloc_disk(NR_MINORS)))
		goto error_after_redister_blkdev;
	blkstat.gendisk->major = major_num;
	blkstat.gendisk->first_minor = 0;
	blkstat.gendisk->fops = &blkstat_ops;
	blkstat.gendisk->private_data = &blkstat;
	strcpy(blkstat.gendisk->disk_name, DEVNAME_0);
	blkstat.gendisk->queue = blkstat.queue;
	add_disk(blkstat.gendisk);

    printk("blkstat: init done\n");

	return blkstat_start(targetname);

error_after_redister_blkdev:
	unregister_blkdev(major_num, DEVNAME);
error_after_alloc_queue:
    blk_cleanup_queue(blkstat.queue);

	return -EFAULT;
}

static void __exit blkstat_exit(void)
{
    printk("blkstat: exit\n");

    if (blkstat.is_active)
    {
        blkdev_put(blkstat.tdev, blkstat_BDEV_MODE);
        bdput(blkstat. tdev);
    }

	del_gendisk(blkstat.gendisk);
	put_disk(blkstat.gendisk);
	unregister_blkdev(major_num, DEVNAME);
	blk_cleanup_queue(blkstat.queue);
}

module_init(blkstat_init);
module_exit(blkstat_exit);
