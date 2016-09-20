
# linux 2.6 kmod makefile for acq2xx host [CPCI backplane] driver
# Makefile from example LDD3 #24

ifeq ($(KERNELRELEASE),)
# default build is the local kernel.
# build other kernels like this example:
# make KRNL=2.6.20-1.2948.fc6-i686 ARCH=i386
# make KRNL=2.6.18-194.26.1.el5 ARCH=i386

KRNL ?= $(shell uname -r)
SRC := $(shell pwd)
# KERNELRELEASE is NOT defined so we were called directly 
# from the command line;
# invoke the kernel build system
# FCx, RHEL:
KERNELDIR ?= /lib/modules/$(KRNL)/build
# traditional systems:
#	KERNELDIR ?= /usr/src/linux/

# release stuff
DC := $(shell date +%Y%m%d%H%M)
TAG := acq2xx_linux26_drv-$(DC)
PACKAGE_FILE := $(TAG).tgz
BINPACK_FILE := $(TAG).bin.tgz

DDRV = acq200_hostdrv.ko

BINFILES:= COPYING README HTSTREAM.README \
	Doxyfile Makefile acqX00.hotplug 20-acqX00.rules links \
	acqcmd htstream setup.htstream.* \
	$(DDRV)

FILES := COPYING README HTSTREAM.README \
	Doxyfile Makefile acqX00.hotplug 20-acqX00.rules links \
	*.[ch] 



all: links apps modules

apps:	acqcmd htstream int-disable-test




modules:
	@echo run make again in $(KERNELDIR)
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

install: hotplug_install modules_install

modules_install:
	install acqcmd /usr/local/bin 
	mkdir -p /lib/modules/$(KRNL)/d-tacq
	cp $(DDRV)  /lib/modules/$(KRNL)/d-tacq
	echo copy $(DDRV) to /lib/modules/$(KRNL)/d-tacq
	-depmod $(KRNL)



hotplug_install:
	chmod u+x acqX00.hotplug
	mkdir -p /etc/hotplug.d/acqX00
	cp acqX00.hotplug /etc/hotplug.d/acqX00
	[ -d /etc/udev/rules.d ] &&  cp 20-acqX00.rules /etc/udev/rules.d


clean:
	@rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions
	@rm -f acqcmd acq200_hostdrv.ko

squeakyclean: clean
	@rm -f `find . -type l`
	@rm links acqcmd.c

# .links target only needed on D-TACQ repo system, links in external files
# package includes all necessary files as regular files
links: 
	@-[ ! -e local.h ] && ln -s ../include/local.h
	@-[ ! -e acq32busprot.h ] && ln -s ../include/acq32busprot.h
	@-[ ! -e acqcmd.c ] && cp ../IPC/acqcmd.c .
	touch links

doc:
	./extract <acq200.c >README
	doxygen

release: doc
	tar cvhzf  $(PACKAGE_FILE) $(FILES) html
	echo cvs tag $(TAG)
	echo $(PACKAGE_FILE) created

bin_rel:
	tar cvhzf  $(BINPACK_FILE) $(BINFILES)
	echo $(BINPACK_FILE) create

acqcmd: acqcmd.o
	$(CC) $(LFLAGS) -o $@ $^ -lpopt

htstream: htstream.o
	$(CC) $(LFLAGS) -o $@ $^ -lpopt
	
int-disable-test: int-disable-test.o
	$(CC) $(LFLAGS) -o $@ $^

.PHONY: modules modules_install clean .links doc release

else
# If KERNELRELEASE is defined, we've been invoked from the 
# kernel build system and can use its language
	obj-m := hw.o

	obj-m := acq200_hostdrv.o

acq200_hostdrv-objs := \
	acq200.o acq200_mmap.o acq200_if.o acq200_core.o acq200_i2o.o \
	acq200_bigbuf_alloc.o \
	acq200_utils.o

endif
