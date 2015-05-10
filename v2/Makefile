
obj-m := ehdrv.o


ehdrv-objs := kernel.o

ifneq ($(KERNELDIR),)
else
KERNELDIR=/lib/modules/`uname -r`/build
endif

#EXTRA_CFLAGS += -Idrivers/media/dvb/dvb-core
#EXTRA_CFLAGS += -Idrivers/media/dvb/frontends

default::
	$(MAKE) -C $(KERNELDIR) SUBDIRS=`pwd` modules
	$(CROSS_COMPILE)strip --strip-debug *.ko
#	$(CROSS_COMPILE)gcc -o user user.c
#	$(CROSS_COMPILE)gcc -o user2 user2.c

clean::
	$(MAKE) -C $(KERNELDIR) SUBDIRS=`pwd` clean
	rm -f user user2

