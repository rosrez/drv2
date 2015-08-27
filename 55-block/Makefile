# out-of-tree pass

ifeq ($(KERNELRELEASE),)

    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD := $(shell pwd)

    INCLUDE_DIR := $(PWD)/../include
    EXTRA_CFLAGS := -I$(INCLUDE_DIR)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) \
            EXTRA_CFLAGS="$(EXTRA_CFLAGS)" modules

modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) \
            EXTRA_CFLAGS="$(EXTRA_CFLAGS)" modules_install

clean:
	@rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.symvers *.unsigned modules.order

.PHONY: modules modules_install clean

#in-tree pass

else

    obj-m := blkstat.o 

endif
