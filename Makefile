
obj-m := gendrv.o


gendrv-objs := gnKernel_anurag_chugh.o

ifneq ($(KERNELDIR),)
else
KERNELDIR=/lib/modules/`uname -r`/build
endif

#EXTRA_CFLAGS += -Idrivers/media/dvb/dvb-core
#EXTRA_CFLAGS += -Idrivers/media/dvb/frontends

default::
	$(MAKE) -C $(KERNELDIR) SUBDIRS=`pwd` modules
	$(CROSS_COMPILE)strip --strip-debug *.ko
	$(CROSS_COMPILE)gcc -o gnUser gnUser.c

clean::
	$(MAKE) -C $(KERNELDIR) SUBDIRS=`pwd` clean

