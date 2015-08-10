#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/pci.h>
#include <linux/input.h>
#include <linux/platform_device.h>

static struct input_dev *vms_input_dev;     /* Representation of an input device */
static struct platform_device *vms_dev;     /* Device structure */

/* sysfs method to input simulated coordinnates to the virtual mouse driver */
static ssize_t write_vms(struct device *dev, struct device_attribute *attr, const char *buffer, size_t count)
{
    int x,y;

    sscanf(buffer, "%d%d", &x, &y);
    input_report_rel(vms_input_dev, REL_X, x);
    input_report_rel(vms_input_dev, REL_Y, y);
    input_sync(vms_input_dev);

    return count;
}

/* Attach the sysfs write method */
DEVICE_ATTR(coordinates, 0644, NULL, write_vms);

/* Attribute descriptor */
static struct attribute *vms_attrs[] = {
    &dev_attr_coordinates.attr,
    NULL
};

/* Attribute group */
static struct attribute_group vms_attr_group = {
    .attrs = vms_attrs
};

/* Driver initialization */
int __init vms_init(void)
{
    int rc;

    /* Register a platform device */
    vms_dev = platform_device_register_simple("vmouse", -1, NULL, 0);
    if (IS_ERR(vms_dev)) {
        PTR_ERR(vms_dev);
        pr_info("vms_init: error\n");
    }

    /* Create a sysfs node to read simulated coordinates */
    rc = sysfs_create_group(&vms_dev->dev.kobj, &vms_attr_group);
    if (rc)
        goto out;

    /* Allocate an input device data structure */
    vms_input_dev = input_allocate_device();
    if (!vms_input_dev)
        pr_info("input_alloc_device() failed\n");

    /* Announce that the virtual mouse will generate relative coordinates */
    set_bit(EV_REL, vms_input_dev->evbit);
    set_bit(REL_X, vms_input_dev->relbit);
    set_bit(REL_Y, vms_input_dev->relbit);

    /* Register with the input subsystem */
    rc = input_register_device(vms_input_dev);
    if (rc)
        goto out;

    pr_info("%s initialized\n", __stringify(KBUILD_NAME));

    return 0;

out:
    if (vms_input_dev)
        input_free_device(vms_input_dev);

    return 0;
}


/* Driver exit */
void vms_exit(void)
{
    /* Unregister from the input subsystem */
    input_unregister_device(vms_input_dev);

    /* Cleanup sysfs node */
    sysfs_remove_group(&vms_dev->dev.kobj, &vms_attr_group);

    /* Unregister driver */
    platform_device_unregister(vms_dev);
}

module_init(vms_init);
module_exit(vms_exit);

MODULE_AUTHOR("OR");
MODULE_LICENSE("GPL v2");
