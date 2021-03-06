ifneq ($(KERNELRELEASE),)
	obj-m := sunfs.o
	sunfs-objs := super.o tools.o inode.o sunfs_buddysystem.o \
		sunfs_filepgt.o file.o log.o
EXTRA_CFLAGS := -DTEST_DEBUG  -ggdb -O0
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	rm *.order *.symvers *.mod.c *.o .*.o.cmd .*.cmd .tmp_versions -rf
endif
