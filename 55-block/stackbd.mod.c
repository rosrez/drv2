#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x14522340, "module_layout" },
	{ 0x4f1939c7, "per_cpu__current_task" },
	{ 0x573939cb, "alloc_disk" },
	{ 0xe5a6404e, "blk_cleanup_queue" },
	{ 0x6980fe91, "param_get_int" },
	{ 0x4d2a8571, "blk_queue_max_hw_sectors" },
	{ 0xc8b57c27, "autoremove_wake_function" },
	{ 0xff964b25, "param_set_int" },
	{ 0xf5bf4d0f, "lookup_bdev" },
	{ 0x3b700c52, "blk_alloc_queue" },
	{ 0xea147363, "printk" },
	{ 0xcf08c5b6, "kthread_stop" },
	{ 0xecde1418, "_spin_lock_irq" },
	{ 0xce4db10e, "del_gendisk" },
	{ 0xeb4c1fc8, "__tracepoint_block_remap" },
	{ 0xb4390f9a, "mcount" },
	{ 0x71a50dbc, "register_blkdev" },
	{ 0xd5ed5307, "generic_make_request" },
	{ 0x4c75ad64, "bio_endio" },
	{ 0xb5a459dc, "unregister_blkdev" },
	{ 0x78764f4e, "pv_irq_ops" },
	{ 0x9fafa033, "blkdev_put" },
	{ 0x41210112, "blk_queue_make_request" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x1000e51, "schedule" },
	{ 0x8aa14eee, "put_disk" },
	{ 0x266c7c38, "wake_up_process" },
	{ 0x4101bbde, "param_set_copystring" },
	{ 0x642e54ac, "__wake_up" },
	{ 0xd2965f6f, "kthread_should_stop" },
	{ 0xc185e3ce, "kthread_create" },
	{ 0x33d92f9a, "prepare_to_wait" },
	{ 0xd7936fb1, "add_disk" },
	{ 0x5c8cbbc8, "set_user_nice" },
	{ 0x9ccb2622, "finish_wait" },
	{ 0xcad9e174, "bdget" },
	{ 0x49e182c0, "param_get_string" },
	{ 0xdeb9153, "blkdev_get" },
	{ 0xe40b105c, "blk_queue_logical_block_size" },
	{ 0x3302b500, "copy_from_user" },
	{ 0x87846fce, "bd_claim" },
	{ 0xd3362be8, "bdput" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "3F524D7E3FC4A27A2D45E55");

static const struct rheldata _rheldata __used
__attribute__((section(".rheldata"))) = {
	.rhel_major = 6,
	.rhel_minor = 4,
};
