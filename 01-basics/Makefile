# To build modules outside of the kernel tree, we run "make"
# in the kernel source tree; the Makefile these then includes this
# Makefile once again.
# This conditional selects whether we are being included from the
# kernel Makefile or not.
ifeq ($(KERNELRELEASE),)

    # Assume the source tree is where the running kernel was built
    # You should set KERNELDIR in the environment if it's elsewhere
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    # The current directory is passed to sub-makes as argument
    PWD := $(shell pwd)

    # As we are not in the kernel source tree; we can get the
    # the current dir of our makefile and set other 
    # required paths relative to it
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

else

# dependencies for individual modules

    # called from kernel build system: just declare what our modules are
    # note that obj-m is treated as a command (that's why it can be indented)
    obj-m := hello_world.o \
		parameters.o \
		cdev.o
endif
