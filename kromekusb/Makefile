TARGET = "kromekusb.ko"
obj-m := kromekusb.o
ifndef KERNEL_SRC
KERNEL_SRC := /lib/modules/$(shell uname -r)/build
endif

SRC := $(shell pwd)
all:
	$(MAKE) -C $(KERNEL_SRC) KBUILD_EXTMOD=$(SRC) modules

install: modules_install
modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers