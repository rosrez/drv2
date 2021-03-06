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
#include <linux/proc_fs.h>
#include <linux/sort.h>

#include <asm/atomic.h>

#include "ififo.h"

#define BLKSTAT_DEBUG 1

#define DEVNAME "blkstat"
#define PROC_ENTRY "blkstat"

#define NR_MINORS 1

#define TDEV_MODE (FMODE_READ | FMODE_WRITE | FMODE_EXCL)
#define KERNEL_SECTOR_SIZE 512  /* FIXME: */

#define MIN_SAMPLES 100
#define DEF_SAMPLES 100000

static int majornr = 0;

static int LOGICAL_BLOCK_SIZE = 512;
module_param(LOGICAL_BLOCK_SIZE, int, S_IRUGO | S_IWUSR);

static int nrsamples = DEF_SAMPLES;
module_param(nrsamples, int, S_IRUGO | S_IWUSR);

char targetname[256];
module_param_string(target, targetname, sizeof(targetname), 0);

/* placeholder for extra data assigned to bi_private of cloned bios */
struct biostat {
    struct bio *bio;        /* original bio */
    unsigned long time;     /* timestamp of submission of cloned bio */
};

struct binfo {
    unsigned long ios[2];
    unsigned long duration[2];
    /* no explicit mean value - can be derived from the above */
    unsigned long minrt;
    unsigned long maxrt;
};

/* our 'device' structure */
struct blkstat {
    /* target device */
    struct block_device *tdev;

    /* the boilerplate stuff: gendisk, queue */
    struct gendisk *gendisk;
    struct request_queue *queue;
    sector_t capacity;

    /* the statistics: info + qdepth + rtimes -- protected by info_lock */
    struct binfo info;
    atomic_t qdepth;
    /* a FIFO with response times (the capacity is nrsamples) */
    struct ififo *rtimes;   

    /* protects the info struct */
    spinlock_t info_lock;
    /* prevents concurrent access to procfs entry */
    struct mutex procfs_mutex;

    int ready;
}; 

/* quantile points, scaled to 100000 total */
#define NR_QUANTILES 11
#define QT_MEDIAN 4

int pval[NR_QUANTILES] = {10000, 20000, 30000, 40000, 50000, 60000, 70000, 80000, 90000, 99000, 99999};
char *pnam[NR_QUANTILES] = {"10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "99%", "99.999%"};

/* a snapshot of statistics from blkstat -- this is presented to user via procfs */
struct userinfo {
    struct binfo info;
    int qdepth;
    int qtles[NR_QUANTILES];
    int rtlen; 
};

static struct blkstat blkstat = {
    .info.minrt = ~0UL,
    .qdepth = ATOMIC_INIT(0)
};

static struct userinfo userinfo;

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

/* must be called with blkstat.info_lock held */
void update_info(int rw, unsigned long rtime)
{
    blkstat.info.ios[rw]++;
    blkstat.info.duration[rw] += rtime;

    if (blkstat.info.maxrt < rtime)
        blkstat.info.maxrt = rtime;
    if (blkstat.info.minrt > rtime)
        blkstat.info.minrt = rtime;
    atomic_dec(&blkstat.qdepth);
    
    if (ififo_is_full(blkstat.rtimes))
        ififo_get(blkstat.rtimes, NULL);

    ififo_put(blkstat.rtimes, rtime);
}

static void blkstat_endio(struct bio *cloned_bio, int error)
{
    unsigned long flags;

    struct biostat *bs = cloned_bio->bi_private;
    struct bio *bio = bs->bio;
   
    unsigned long now = ktime_to_ns(ktime_get());
    unsigned long nselapsed = now - bs->time;

    int uptodate = test_bit(BIO_UPTODATE, &cloned_bio->bi_flags);

    pr_info("%s: endio -- elapsed: %lu -- size: %u -- up-to-date: %d-- error: %d -- IRQ: %lu -- INTR: %lu\n", 
        DEVNAME, nselapsed, cloned_bio->bi_iter.bi_size, uptodate, error, in_irq(), in_interrupt());

    /*
     * It seems that we may get called both from a non-IRQ and IRQ context,
     * so use the irq-saving version, to be on the safe side.
     */
    spin_lock_irqsave(&blkstat.info_lock, flags);
    update_info(bio_data_dir(cloned_bio) & REQ_WRITE, nselapsed);
    spin_unlock_irqrestore(&blkstat.info_lock, flags);

    bio_endio(bio, error);
    free_biostat(bs);    
}

static void blkstat_make_request(struct request_queue *q, struct bio *bio) 
{
    struct bio *cloned_bio;

#ifdef BLKSTAT_DEBUG 
    pr_info("blkstat: make request %-5s block %-12llu #pages %-4hu total-size "
            "%-10u\n", bio_data_dir(bio) == WRITE ? "write" : "read",
            (unsigned long long) bio->bi_iter.bi_sector, bio->bi_vcnt, bio->bi_iter.bi_size);
#endif

    if (!blkstat.tdev)
    {
        pr_info("blkstat: Request before tdev is ready, aborting\n");
        goto err_bio;
    }
    if (!blkstat.ready)
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

    atomic_inc(&blkstat.qdepth);
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

    if (blkdev_get(tdev, TDEV_MODE, &blkstat))
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

    blkstat.ready = 1;

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

static struct block_device_operations blkstat_ops = {
    .owner  = THIS_MODULE,
	.getgeo = blkstat_getgeo,
};

static void *blkstat_seq_start(struct seq_file *sf, loff_t *pos)
{

    int cmp(const void *l, const void *r)
    {   
        return *((int *) l) - *((int *) r); 
    }

    int *samples;
    unsigned long flags;
    int qtloc;
    int len, i;

    /* 
     * Prevent concurrent access to the seq_file. Race to perform a vmalloc() 
     * by concurrent invocations would be a disaster. May also corrupt 
     * statistics data while output is in progress. Obviously, we take out 
     * the sleeping lock (mutex) prior to acquiring the spinlock.
     */
    if (mutex_lock_interruptible(&blkstat.procfs_mutex))
        return NULL;    /* signal termination to the higher layer */

    if (*pos == 0) {
        /* 
         * Note the race here: we use the length value later, in the critical section,
         * when the FIFO length may have changed. This is hardly a problem
         * since our FIFO either grows in size or remains at the maximum size level.
         * We can't do it otherwise because vmalloc() may sleep and is not allowed
         * in our spinlock-protected section.
         */
        len = ififo_len(blkstat.rtimes);
        /* chose vmalloc() to put less pressure on system memory for large sample sets */ 
        samples = vmalloc(len * sizeof(*samples));

        /* grab the info spinlock and take a snapshot of current statistics */
        spin_lock_irqsave(&blkstat.info_lock, flags);
        memcpy(&userinfo.info, &blkstat.info, sizeof(userinfo));
        userinfo.qdepth = atomic_read(&blkstat.qdepth);
        userinfo.rtlen = min(len, ififo_len(blkstat.rtimes));
        for (i = 0; i < userinfo.rtlen; i++)
            ififo_get_at(blkstat.rtimes, &samples[i], i);
        spin_unlock_irqrestore(&blkstat.info_lock, flags);
        /* info spinlock -- end of critical section */

        sort(samples, userinfo.rtlen, sizeof(int), &cmp, NULL);

        for (i = 0; i < NR_QUANTILES; i++) {
            /* rescale quantile ranks */
            qtloc = pval[i] * userinfo.rtlen / 100000;
            /* store values for subsequent access by seq_file methods */
            userinfo.qtles[i] = samples[qtloc];
        }

        vfree(samples); 
    
        return SEQ_START_TOKEN;
    }

    return NULL;
}

static void *blkstat_seq_next(struct seq_file *sf, void *v, loff_t *pos)
{
    long idx = (long) v;
    idx++;
    (*pos)++;

    if (idx - 2 < NR_QUANTILES)
        return (void *) idx;
    else
        return NULL;
}

static void blkstat_seq_stop(struct seq_file *sf, void *v)
{
    mutex_unlock(&blkstat.procfs_mutex);
}

static int blkstat_seq_show(struct seq_file *sf, void *v)
{
    struct binfo *info = &userinfo.info;
    long idx = (long) v;

    /* the first item => header */
    if (idx == 1) {
        long meanrt = 0;
        if (info->ios[0] + info->ios[1])
            meanrt = (info->duration[0] + info->duration[1]) / (info->ios[0] + info->ios[1]);
        seq_printf(sf, "Target device: %s\n", targetname);
        seq_printf(sf, "Read I/Os: %lu", info->ios[0]);
        if (info->ios[0]) 
            seq_printf(sf, " -- I/Os per sec: %lu\n", info->duration[0]/info->ios[0]);
        else
            seq_printf(sf, "\n");
        seq_printf(sf, "Write I/Os: %lu", info->ios[1]);
        if (info->ios[1])
            seq_printf(sf, " -- I/Os per sec: %lu\n", info->duration[1]/info->ios[1]);
        else
            seq_printf(sf, "\n");
        seq_printf(sf, "Queue depth: %d\n", userinfo.qdepth);
        seq_printf(sf, "I/O service time (ns)\n");
        seq_printf(sf, "Min: %lu -- Max: %lu\n", info->minrt, info->maxrt);
        seq_printf(sf, "Mean: %lu\n", meanrt);
        seq_printf(sf, "Median: %d\n", userinfo.qtles[QT_MEDIAN]);
        
        return 0;
    }

    /* adjust the index: 1 is the header, 2 is start of data */
    idx -= 2;

    seq_printf(sf, "%-7s:", pnam[idx]);
    seq_printf(sf, " %d\n", userinfo.qtles[idx]);
    return 0;
}

static struct seq_operations seq_ops = {
    .start  = blkstat_seq_start,
    .next   = blkstat_seq_next,
    .stop   = blkstat_seq_stop,
    .show   = blkstat_seq_show,
};

static int blkstat_seq_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &seq_ops);
}

static struct file_operations proc_fops = {
    .open       = blkstat_seq_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = seq_release,
};

static struct proc_dir_entry *proc_entry;

static int __init blkstat_init(void)
{
    int rc;

    if (strcmp(targetname, "") == 0)
        return -EINVAL;

    if (nrsamples < MIN_SAMPLES)
        return -EINVAL;

    spin_lock_init(&blkstat.info_lock);
    mutex_init(&blkstat.procfs_mutex);

    /* allocate the FIFO to store recent response times */
    if ((rc = ififo_alloc(&blkstat.rtimes, nrsamples, GFP_KERNEL)))
        return rc;

	/* 
     * Use blk_alloc_queue() to set up the 'bio-oriented' processing,
     * i.e. it is enough to register the make_request() callback
     * that deals with individual bios.
     * Otherwise, we would have to use the 'request-oriented' processing.
     */
    if (!(blkstat.queue = blk_alloc_queue(GFP_KERNEL))) {
        pr_info("%s: alloc_queue failed\n", DEVNAME);
        goto error_rm_fifo;
    }

    /* register our make_request() callback */
    blk_queue_make_request(blkstat.queue, blkstat_make_request);
    /* report our block size */
	blk_queue_logical_block_size(blkstat.queue, LOGICAL_BLOCK_SIZE);

	/* Register the driver for our logical device */
	if ((majornr = register_blkdev(majornr, DEVNAME)) < 0) {
		pr_info("%s: unable to get major number\n", DEVNAME);
		goto error_rm_queue;
	}

	/* Populate the gendisk structure */
	if (!(blkstat.gendisk = alloc_disk(NR_MINORS)))
		goto error_rm_dev;

	blkstat.gendisk->major = majornr;
	blkstat.gendisk->first_minor = 0;
	blkstat.gendisk->fops = &blkstat_ops;
	blkstat.gendisk->private_data = &blkstat;
	snprintf(blkstat.gendisk->disk_name, 32, "%s%d", DEVNAME, 0); 
	blkstat.gendisk->queue = blkstat.queue;
	add_disk(blkstat.gendisk);

    pr_info("%s: disk init done\n", DEVNAME);

	rc = blkstat_start(targetname);

    proc_entry = proc_create(PROC_ENTRY, S_IRUGO, NULL, &proc_fops);
    return rc;

error_rm_fifo:
    ififo_free(blkstat.rtimes);

error_rm_dev:
	unregister_blkdev(majornr, DEVNAME);

error_rm_queue:
    blk_cleanup_queue(blkstat.queue);
	return -ENXIO;
}

static void __exit blkstat_exit(void)
{
    remove_proc_entry(PROC_ENTRY, NULL);

    if (blkstat.ready) {
        blkdev_put(blkstat.tdev, TDEV_MODE);
        bdput(blkstat.tdev);
    }

    if (blkstat.gendisk) {
       del_gendisk(blkstat.gendisk);
       put_disk(blkstat.gendisk);
    }

    if (blkstat.queue)
	    blk_cleanup_queue(blkstat.queue);

    ififo_free(blkstat.rtimes);	
    unregister_blkdev(majornr, DEVNAME);
    pr_info("%s: exit complete\n", DEVNAME);
}

module_init(blkstat_init);
module_exit(blkstat_exit);

MODULE_AUTHOR("Oleg Rosowiecki");
MODULE_LICENSE("GPL");
