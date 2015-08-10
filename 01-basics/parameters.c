#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

#define PARM_ARRAY_SIZE (8)

int par1;
int par2;

int arr1[PARM_ARRAY_SIZE];
//int arrcnt1;
char str1[32];

/* module_param(name, type, perm);
 * perm is for accessing the corresponding entry in sysfs from userspace */
module_param(par1, int, 0);

/* module_param_named(name, value, type, perm);
 * name is used to 'alias' the module's internal variable in interactions with userspace (insmod/sysfs) */
module_param_named(named1, par2, int, 0);

/* module_param_array(name, array, type, num, perm);
 * num is the number of elements in array */
module_param_array(arr1, int, NULL, 0);

/* module_param_string(name, string, len, perm); */
module_param_string(string1, str1, sizeof(str1), 0); 

static int __init my_init(void)
{
    char    tmp[256];
    char    *s = &tmp[0];
    int     i;

    for (i = 0; i < PARM_ARRAY_SIZE; i++) {
        s += sprintf(s, "%d,", arr1[i]);
    }
    *(--s) = '\0';

    printk(KERN_INFO "Module loaded at 0x%p\n", my_init);
    printk(KERN_INFO "par1 = %d\n", par1);
    printk(KERN_INFO "par2 = %d\n", par2);
    printk(KERN_INFO "arr1 = {%s}\n", tmp);
    printk(KERN_INFO "str1 = %s", str1);
    return 0;
}

static void __exit my_exit(void)
{
    printk(KERN_INFO "Module unloaded from 0x%p\n", my_exit);
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("MODULE AUTHOR");
MODULE_LICENSE("GPL v2");
