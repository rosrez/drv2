#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h> 
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>

#define DEVNAME_0 "blkstat0"
#define DEVNAME "blkstat"

#define NR_MINORS 1

#define STACKBD_BDEV_MODE (FMODE_READ | FMODE_WRITE | FMODE_EXCL)
#define KERNEL_SECTOR_SIZE 512  /* FIXME: */

static int majornr = 0;

static int LOGICAL_BLOCK_SIZE = 512;
module_param(LOGICAL_BLOCK_SIZE, int, S_IRUGO | S_IWUSR);

/* FIXME: set the default to 100000 */
static int nrsamples = 100;
module_param(nrsamples, int, S_IRUGO | S_IWUSR);

char targetname[256];
module_param_string(target, targetname, sizeof(targetname), 0);

/* placeholder for extra data assigned to bi_private of cloned bios */
struct biostat {
    struct bio *bio;        /* original bio */
    unsigned long time;     /* timestamp of submission of cloned bio */
};

struct binfo {
    int ios[2];
    int totrt[2];
    /* no explicit mean value - can be derived from the above */
    int minrt;
    int maxrt;
/* FIXME: remove comment */
#if 0
    struct ififo *rt;   /* a FIFO with response times (the capacity is nrsamples) */
#endif
};

/* our 'device' structure, contains... everyting */
struct blkstat {
    /* target device */
    struct block_device *tdev;

    /* the usual boilerplate stuff: gendisk, queue */
    struct gendisk *gendisk;
    struct request_queue *queue;
    sector_t capacity;

    /* the statistics structure */
    struct binfo binfo;

    /* protects the binfo structure */
    spinlock_t slock;

    /* protects the blkstat structure */
    mutex_t mlock;

    int is_active;  /* FIXME: remove this */
}; 

static struct blkstat blkstat;

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
   
    unsigned long now = ktime_to_ns(ktime_get());
    unsigned long nselapsed = now - bs->time;

    int uptodate = test_bit(BIO_UPTODATE, &cloned_bio->bi_flags);

    pr_info("%s: endio -- elapsed: %lu -- size: %u -- up-to-date: %d-- error: %d -- IRQ: %lu -- INTR: %lu\n", 
        DEVNAME, nselapsed, cloned_bio->bi_iter.bi_size, uptodate, error, in_irq(), in_interrupt());

    bio_endio(bio, error);
    free_biostat(bs);    
}

static void blkstat_make_request(struct request_queue *q, struct bio *bio) 
{
    struct bio *cloned_bio;

    pr_info("blkstat: make request %-5s block %-12llu #pages %-4hu total-size "
            "%-10u\n", bio_data_dir(bio) == WRITE ? "write" : "read",
            (unsigned long long) bio->bi_iter.bi_sector, bio->bi_vcnt, bio->bi_iter.bi_size);

    pr_info("<%p> Make request %s %s %s\n", bio,
           bio->bi_rw & REQ_SYNC ? "SYNC" : "",
           bio->bi_rw & REQ_FLUSH ? "FLUSH" : "",
           bio->bi_rw & REQ_NOIDLE ? "NOIDLE" : "");

    if (!blkstat.tdev)
    {
        pr_info("blkstat: Request before tdev is ready, aborting\n");
        goto err_bio;
    }
    if (!blkstat.is_active)
    {
        pr_info("blkstat: Device not active yet, aborting\n");
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
    return;

err_free_clone:
    bio_put(cloned_bio);

err_bio:
    bio_io_error(bio);
    return;
}

static struct block_device *blkstat_bdev_open(char dev_path[])
{
    /* Open underlying device */
    struct block_device *tdev = lookup_bdev(dev_path);
    if (IS_ERR(tdev))
    {
        pr_info("blkstat: error opening raw device <%lu>\n", PTR_ERR(tdev));
        return NULL;
    }

    if (!bdget(tdev->bd_dev))
    {
        pr_info("blkstat: error bdget()\n");
        return NULL;
    }

    if (blkdev_get(tdev, STACKBD_BDEV_MODE, &blkstat))
    {
        pr_info("blkstat: error blkdev_get()\n");
        bdput(tdev);
        return NULL;
    }

    pr_info("Opened %s\n", dev_path);

    return tdev;
}

static int blkstat_start(char dev_path[])
{
    unsigned max_sectors;

    if (!(blkstat.tdev = blkstat_bdev_open(dev_path)))
        return -EFAULT;

    /* Set up our internal device */
    blkstat.capacity = get_capacity(blkstat.tdev->bd_disk);
    pr_info("blkstat: Device real capacity: %llu\n", (unsigned long long) blkstat.capacity);

    set_capacity(blkstat.gendisk, blkstat.capacity);

    max_sectors = queue_max_hw_sectors(bdev_get_queue(blkstat.tdev));
    blk_queue_max_hw_sectors(blkstat.queue, max_sectors);
    pr_info("blkstat: Max sectors: %u\n", max_sectors);

    pr_info("blkstat: done initializing successfully\n");
    blkstat.is_active = 1;

    return 0;
}

/*
 * Apart from capacity, this is a bogus geometry info since 
 * we are not a physical device. But we need to report that to
 * userspace to enable fdisk work with the device. 
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
    if (strcmp(targetname, "") == 0)
        return -EINVAL;

	spin_lock_init(&blkstat.lock);

	/* 
     * Use blk_alloc_queue() to set up the 'bio-oriented' processing,
     * i.e. it is enough to register the make_request() callback
     * that deals with individual bios.
     * Otherwise, we would have to use the 'request-oriented' processing,
     * and register a callback function that deals with individual
     * requests. FIXME: check on request-oriented.
     */
    if (!(blkstat.queue = blk_alloc_queue(GFP_KERNEL)))
    {
        pr_info("blkstat: alloc_queue failed\n");
        return -EFAULT;
    }

    /* register our make_request() callback */
    blk_queue_make_request(blkstat.queue, blkstat_make_request);
    /* report our block size */
	blk_queue_logical_block_size(blkstat.queue, LOGICAL_BLOCK_SIZE);

	/* Register the driver for our logical device */
	if ((majornr = register_blkdev(majornr, DEVNAME)) < 0)
    {
		pr_info("blkstat: unable to get major number\n");
		goto error_after_alloc_queue;
	}

	/* Populate the gendisk structure */
	if (!(blkstat.gendisk = alloc_disk(NR_MINORS)))
		goto error_after_redister_blkdev;

	blkstat.gendisk->major = majornr;
	blkstat.gendisk->first_minor = 0;
	blkstat.gendisk->fops = &blkstat_ops;
	blkstat.gendisk->private_data = &blkstat;
	snprintf(blkstat.gendisk->disk_name, 32, "%s%d", DEVNAME, 0); 
	blkstat.gendisk->queue = blkstat.queue;
	add_disk(blkstat.gendisk);

    pr_info("blkstat: disk init done\n");

	return blkstat_start(targetname);

error_after_redister_blkdev:
	unregister_blkdev(majornr, DEVNAME);

error_after_alloc_queue:
    blk_cleanup_queue(blkstat.queue);
	return -EFAULT;
}

static void __exit blkstat_exit(void)
{
    if (blkstat.is_active) {
        blkdev_put(blkstat.tdev, STACKBD_BDEV_MODE);
        bdput(blkstat.tdev);
    }

    if (blkstat.gendisk) {
       del_gendisk(blkstat.gendisk);
       put_disk(blkstat.gendisk);
    }

    if (blkstat.queue)
	    blk_cleanup_queue(blkstat.queue);
	
    unregister_blkdev(majornr, DEVNAME);
    pr_info("%s: exit complete\n", DEVNAME);
}

module_init(blkstat_init);
module_exit(blkstat_exit);

MODULE_AUTHOR("Oleg Rosowiecki");
MODULE_LICENSE("GPL");
