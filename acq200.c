/* ------------------------------------------------------------------------- */
/* acq200.c driver toplevel for acq200 peripheral                            */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2006-7 Peter Milne, D-TACQ Solutions Ltd
 *                      <Peter dot Milne at D hyphen TACQ dot com>
                                                                               
    This program is free software; you can redistribute it and/or modify
    it under the terms of Version 2 of the GNU General Public License
    as published by the Free Software Foundation;
                                                                               
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
                                                                               
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

/** @file acq200.c <b>core Linux 2.6.x host pci driver for 
	D-TACQ ACQ2xx cards</b>.
 *
 */

/** <!-- README: -->
 

@mainpage "D-TACQ ACQ2xx Linux 2.6 Host PCI Driver $Revision: 1.60 $"

@author Peter Milne <peter.milne@d-tacq.com>
@n
@n
This README file reads best as ./html/index.html
The tags are for processing by Doxygen.
@n
@n
- Products
 - acq200_hostdrv.ko  : Linux 2.6 host side kernel driver module
 - /etc/hotplug.d/acqX00/acqX00.hotplug : hotplug script (valid pre-udev)
 - /etc/udev/rules.d/20-acq2XX.rules : udev hotplug wrapper - 
		(calls acqX00.hotplug)
 - acqcmd.c. : utility to run commands on ACQxxx target card.
 - htstream.c. : hi throughput streaming application.
@n
@n
- Features
 - hotplug - automated device detection. 
 - dynamic host buffer allocation - buffer size typical 4MB.
 - full acqcmd supported
 - remote shell feature supported 
 - memory mapped mailbox registers supported,
	 so high rep rate memory mapped control loops eg llcontrol will work.
 - works with multiple processors and APIC (with kernel support).
@n
@n
- Build instructions
 - we assume a host system for compilation, target system for deployment
  - both systems are likely to be x86 systems running kernel 2.6.20 or newer.
  - it is quite possible that both host and target roles are performed 
     by the same machine.
 - check Makefile to ensure KERNELDIR points to correct kernel headers\n
	or set on the command line as shown below.   \n
	on Fedora systems, 'yum install kernel-devel' fetches the headers
 - on the host system, run: make 
   -eg make KERNELDIR=~/PROJECTS/LINUX26/linux-2.6-x86/
 - on the target system, run: make install
 - to load the driver:
  - /sbin/insmod acq200_hostdrv.ko
@n
- Usage
 - hotplug will create device nodes automatically.
 - use existing acqcmd -s mode to send shell commands, acqcmd -b as before
 - llcontrol will "just work".
@n
- Notes
 - tested on x86 32 bit systems only.
 - tested to work with up to 2GB memory. not tested beyond 2GB.
@n

@page debug Debug Checklist
In the event that the installation doesn't work, follow this checklist step by step. 
-# Is the card visible on the bus? (example for ACQ196)
@code
/sbin/lspci | grep 318
09:0d.0 Co-processor: Intel Corporation: Unknown device 0318 (rev 02)
09:0e.0 Co-processor: Intel Corporation: Unknown device 0318 (rev 02)

@endcode
-# Did the module load?
@code
bash-3.00$ grep acq /proc/modules 
acq200_hostdrv 44740 0 - Live 0xf8bfc000

@endcode
-# Were the logical device[s] created?
@code
bash-3.00$ ls -1 /sys/class/acqX00
acq200.0
acq200.1

bash-3.00$ grep acq200 /proc/devices
252 acq200.1
253 acq200.0
... example shows two ACQ196 devices of the class acqX00
@endcode
-# Can we feel the heartbeat?.
@code
bash-3.00$ cat /sys/class/acqX00/acq200.0/device/heartbeat 
10

... non-zero number means the card is alive

@endcode
-# Does the slot mapping correspond to physical CPCI slots?
@code
bash-3.00$ cat /sys/module/acq200_hostdrv/parameters/slots 
4,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0

in the above example, logical device 
acq200.0 is in CPCI slot 4
acq200.1 is in CPCI slot 3

@endcode
-# Did the hotplug script work?
@code
bash-3.00$ ls -l /dev/acq200/.*rsh
crw-rw-r--  1 root dt100 252, 229 Aug  7 12:35 /dev/acq200/acq200.3.rsh
crw-rw-r--  1 root dt100 253, 229 Aug  7 12:35 /dev/acq200/acq200.4.rsh

# the rsh device is used by acqcmd -s SLOT

bash-3.00$ ls -l /dev/acq32/acq32.?.m?
crw-rw-r--  1 root dt100 252, 237 Aug  7 12:35 /dev/acq32/acq32.3.m3
crw-rw-r--  1 root dt100 253, 237 Aug  7 12:35 /dev/acq32/acq32.4.m4

# the .m master device is used by acqcmd -b BOARD

The major number corresponds to the device number in /proc/devices

NB: the hotplug script REQUIRES a user 'dt100' to exist in /etc/group, by convention, this is:

bash-3.00$ grep dt100 /etc/group
dt100:x:1001:milne

@endcode
-# Were channel data nodes created?
@code
bash-3.00$ ls /dev/acq32/acq32.?.[0-9][0-9]
/dev/acq32/acq32.3.00  /dev/acq32/acq32.3.54  /dev/acq32/acq32.4.43
/dev/acq32/acq32.3.01  /dev/acq32/acq32.3.55  /dev/acq32/acq32.4.44
/dev/acq32/acq32.3.02  /dev/acq32/acq32.3.56  /dev/acq32/acq32.4.45
@endcode
-# Are we able to talk to the card using acqcmd?.
@code
bash-3.00$ acqcmd -b 3 getFwrev
ACQ32:linux.initrd.extra 73.2029.2393
ACQ216CPCI, WAV232CPCI copyright (c) D-TACQ Solutions Ltd 2003-2006
ACQ200 image built at May 11 2007:18:06:30 Build 2201
$Id: acq200.c,v 1.60 2012/01/18 09:34:25 pgm Exp $
$Revision: 1.60 $

bash-3.00$ acqcmd -s 3 'echo `hostname` `get.release.versions`'
acq196_079 73.2029.2393
EOF 17 0

@endcode
-# OK, let's capture some data!
@code
bash-3.00$ acqcmd -b 3 setInternalClock 250000
ACQ32:
bash-3.00$ acqcmd -b 3 setMode SOFT_TRANSIENT 100000
ACQ32:
bash-3.00$ acqcmd -b 3 setArm
ACQ32:
bash-3.00$ acqcmd -b 3 getState
ACQ32:0 ST_STOP
bash-3.00$ acqcmd -b 3 getNumSamples
ACQ32:getNumSamples=100000 pre=0 post=100000 elapsed=100000
bash-3.00$ dd if=/dev/acq32/acq32.3.03 bs=1000000 | hexdump | head
0000000 0021 001d 001e 001e 001e 0021 001d 001f
0000010 001e 001d 0021 001c 001d 001e 001d 0022
0000020 001c 001c 001d 001d 0021 001e 001e 001c
0000030 001c 0020 0020 001d 001e 001b 0020 001f

@endcode
@page license Software License
@code
*/
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2006-7 Peter Milne, D-TACQ Solutions Ltd
 *                      <Peter dot Milne at D hyphen TACQ dot com>
                                                                               
    This program is free software; you can redistribute it and/or modify
    it under the terms of Version 2 of the GNU General Public License
    as published by the Free Software Foundation;
                                                                               
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
                                                                               
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */
/*
@endcode
@page knobs Driver Tweaks
This is a summary of device driver tweaks.
-/sys/module/acq200_hostdrv/parameters/
 - acq200_debug_mbox\n
	set non-zero to debug mailbox transactions
 - acq200_hostdrv_debug\n
	set greater than zero to debug driver in general
 - acq200_stub_copy_to_user\n
	set non-zero for performance test [do not use]
 - acq200_use_interrupts\n
	set zero to disable interrupts [do not use]
 - slots\n
	maps logical device to CPCI slot#

-/sys/class/acqX00/acq200.L/device/ \n
	where L is the logical device number 0..
 - available_input_channels
 - available_output_channels
 - cpci_slot
 - heartbeat\n
	non-zero value indicates "card is alive"
 - host_len\n
	host buffer length
 - host_pa\n
	host buffer physical [bus] address
 - interrupt_count
 - irq
 - pool
 - subsystem_device
 - subsystem_vendor

<!-- /README --> */

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/timex.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>

#include <linux/proc_fs.h>

#include <asm/uaccess.h>  /* VERIFY_READ|WRITE */

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#include <linux/module.h>
#endif

#include "acq32busprot.h"

#include "acq200_debug.h"
#include "acq200.h"
#include "acq200_minors.h"
#include "acq200_ioctl.h"
#include "acq32ioctl.h"
#include "rch.h"

#define MAXDEV 16
#define DEFTO  HZ


/**< parameters - useful, but beware these are global */
int acq200_hostdrv_debug = 0;
module_param(acq200_hostdrv_debug, int, 0644);
MODULE_PARM_DESC(acq200_hostdrv_debug, "debug level [0 = OFF]");
EXPORT_SYMBOL_GPL(acq200_hostdrv_debug);

int acq200_use_interrupts = 1;
module_param(acq200_use_interrupts, int, 0644);
MODULE_PARM_DESC(acq200_use_interrupts, "use interrupts [0 = OFF]");

int acq200_debug_mbox = 0;
module_param(acq200_debug_mbox, int, 0644);
MODULE_PARM_DESC(acq200_debug_mbox, "debug mailbox transactions [0 = OFF]");

int slots[MAXDEV] = { 0, };
module_param_array(slots,int,NULL,0444);
MODULE_PARM_DESC(slots, "maps logical device# to CPCI slot#");

int acq200_stub_copy_to_user = 0;
module_param(acq200_stub_copy_to_user, int, 0644);
MODULE_PARM_DESC(acq200_stub_copy_to_user, "profiling aid [1 = no copy]");


int user_max_buf = 0x300000;		/* MULTIPLE of 192 for 96ch mode */
module_param(user_max_buf, int, 0644);

#define LINUX26_MAXALLOC_PAGE_ORDER 12

/**  communications buffer must have a large minimum size. */
#define COMMSBUF_ORDER	LINUX26_MAXALLOC_PAGE_ORDER

int acq200_AB_dmabuf_max_order = LINUX26_MAXALLOC_PAGE_ORDER;
module_param(acq200_AB_dmabuf_max_order, int, 0644);
/**< Maximum size of A,B buffers in pages. May reduce for lower latency */


/** Globals .. keep to a minimum! */
char acq200_driver_name[] = "acq200";
char acq200__driver_string[] = "D-TACQ intelligent data acquisition device";
char acq200__driver_version[] = "$Revision: 1.60 $ B1114";
char acq200__copyright[] = "Copyright (c) 2006/7 D-TACQ Solutions Ltd";
static struct Acq200Device* devices[MAXDEV];
static int idx;


struct class* acq200_device_class;

struct Acq200Device* acq200_lookupDevice(struct device *dev)
{
	int id;

	for (id = 0; id < idx; ++id){
		struct Acq200Device* acq200_device = devices[id];
		if (dev == &devices[id]->pci_dev->dev){
			return acq200_device;
		}
	}
	
	return 0;
}

struct Acq200Device *acq200_lookupDeviceFromClass(struct CLASS_DEVICE *dev)
{
	int id;

	for (id = 0; id < idx; ++id){
		struct Acq200Device* acq200_device = devices[id];
		if (dev == devices[id]->class_dev){
			return acq200_device;
		}
	}
	
	return 0;
}


static void get_max_buf(struct DmaBuf *dma_buf, int max_order)
{
	void *buf;
	int order;

	for (order = max_order; order >= 0; --order){
		buf = (void*)__get_free_pages(GFP_KERNEL|GFP_DMA32|__GFP_NOWARN, order);
		if (buf){
			dbg(1, "%2d [0x%08lx bytes] SUCCESS",
			       order, ORDER2LEN(order));
			dma_buf->va = buf;
			dma_buf->order = order;
			return;
		}else{
			dbg(2, "%2d [0x%08lx bytes] FAIL",
			       order, ORDER2LEN(order));
		}
	}
	assert(0);
}

static void alloc_max_buf(
	struct device *dev, 
	struct DmaBuf *dma_buf, 
	enum dma_data_direction dir,
	int max_order)
{
	get_max_buf(dma_buf, max_order);
	dma_buf->pa = dma_map_single(
			dev, dma_buf->va, dma_buf_len(dma_buf), dir);
}

static void alloc_llhost_buf(
	struct device *dev,
	struct DmaBuf *dma_buf)
{
	dma_addr_t pa;
	dma_buf->va = dma_alloc_coherent(dev, PAGE_SIZE, &pa,
				GFP_KERNEL|GFP_DMA32|__GFP_NOWARN);
	dma_buf->order = 0;
	dma_buf->pa = pa;
}

static 
struct Acq200Path *acq200_createPath(
	struct inode* inode, struct Acq200Device *device) 
{
	struct Acq200Path *path = 
		kmalloc(sizeof(struct Acq200Path), GFP_KERNEL);

	memset(path, 0, PATH_HEAD);

	if (path != 0){
		path->device = device;
		path->minor = MINOR(inode->i_rdev);
		path->dma_buf = device->dma_buf;
		path->llhost_dma_buf = device->llhost_dma_buf;
		init_waitqueue_head(&path->return_waitq);
	}
	return path;
}

static void acq200_deletePath(struct Acq200Path *path)
{
	kfree(path);
}
static struct Acq200Device *acq200_lookup_device(struct inode* inode)
{
	int major = MAJOR(inode->i_rdev);
	int isearch;

	dbg(2, "major %d", major);

	for (isearch = 0; isearch < idx; ++isearch){
		struct Acq200Device* device = devices[isearch];

		assert(device != 0);

		if (device->ldev.major == major){
			return device;
		}
	}

	assert(0);
	return 0;
}


/* set a BIG timeout to handle case of timer ints disabled */
#define BIGTIME 0x7fffffff   

static int acq200_poll_response(
	struct Acq200Device* device, long timeout)
{
	unsigned itimeout;
	unsigned mask = BP_CI_ACK;
	unsigned long end = jiffies + timeout;

	for (itimeout = 0; itimeout != BIGTIME; ++itimeout){
		u32 ucommand = ioread32(&MBOX(device, BP_MB_COMMAND));

		if ((ucommand&mask) != 0){
			return 0;
		}else{
			if (itimeout < 4){
				dbg(2, "call to schedule STUBBED\n");
			}else{
				schedule();
			}
		}
		if (time_after(jiffies, end)){
			break;
		}
	}
 
	return -ETIMEDOUT;
}

static void fill_ramp(int *data, int len)
{
	int ilen = len / sizeof(int);
	int ii;
	unsigned id = idx << 28;

	for (ii = 0; ii < ilen; ++ii){
		data[ii] = id | ii;
	}
}



static struct Acq200Device *acq200_device_create(struct pci_dev *pci_dev)
{
	struct Acq200Device *device =
		kmalloc(sizeof(struct Acq200Device), GFP_KERNEL);

	assert(device);
	memset(device, 0, sizeof(struct Acq200Device));
	assert(idx < MAXDEV);

	devices[idx] = device;
	device->pci_dev = pci_dev;
	device->idx = idx++;
	sprintf(device->ldev.drv_name, "acq200.%d", device->idx );

	alloc_llhost_buf(&pci_dev->dev, &device->llhost_dma_buf);

	alloc_max_buf(&pci_dev->dev, &device->dma_buf, DMA_BIDIRECTIONAL, 
			COMMSBUF_ORDER);
	fill_ramp(device->dma_buf.va, dma_buf_len(&device->dma_buf));

	acq200_map_pci_memory(device);

/** acq32 i2o buffers are acq32-side, but acq200 are host side,
 * this is for emulation.
 */
	device->ram.va = device->dma_buf.va;
	device->ram.pa = device->dma_buf.pa;

	init_MUTEX(&device->mbox_sync.mutex);
	init_MUTEX(&device->clients.mutex);
	init_MUTEX(&device->buffer_AB_busy_mutex);
	init_waitqueue_head(&device->mbox_sync.waitq);
	init_waitqueue_head(&device->m_dpd.aux_waitq);
	streamBufInit(&device->streambuf);
	tasklet_init(
		&device->m_dpd.streaming_isr_tasklet, 
		acq200_streaming_isr_bh, 
		(unsigned long) device);

	rchInit(device, 0);
	acq200_pool_create(device);
	return device;
}

static void __kbuf_free(struct DmaBuf *dma_buf)
{
	if (dma_buf->va != 0){
		__free_pages(dma_buf->va, dma_buf->order);
	}
}
static void acq200_device_free(struct Acq200Device *device)
{
	acq200_pool_delete(device);
	acq200_unmap_pci_memory(device);

	__free_pages(device->llhost_dma_buf.va, device->llhost_dma_buf.order);
	__kbuf_free(&device->dma_buf);
	__kbuf_free(&device->AB[0]);
	__kbuf_free(&device->AB[1]);
	kfree(device);
}



static ssize_t acq200_read (
	struct file* file, char* buf, size_t count, loff_t* posp)
{
        char myline[80];
	struct Acq200Device *device = DEV(file);
        int pos = (int)*posp;

        int len = sprintf(myline, "BIGBUF: pa=0x%08x len=0x%08lx\n",
			  device->dma_buf.pa, 
			  ORDER2LEN(device->dma_buf.order));

        if (pos < len){
		int rc;

                len = min((int)count, len - pos);
                rc = copy_to_user(buf, myline + pos, len);
		if (rc != 0){
			err("returning EFAULT");
			return -EFAULT;
		}
                *posp += len;
                return len;
        }else{
                return 0;
        }
}

static ssize_t acq200_write (
	struct file *file, const char *buf, size_t count, loff_t * posp)
// write data as if from debug port
{	
	dbg(1, "count %d", (int)count);

	return 0;
}



#define MBX_READ 1

static void acq200_mbox_dbg(struct MailboxControl *mbc, int read)
{
	u32* regs = read? mbc->mbxr: mbc->mbxw;
	unsigned mask = read? MBC_FROM_MBXR(mbc):  MBC_FROM_MBXW(mbc);

	info("%5s: [%x] %08x %08x %08x %08x", 
		read? "READ": "WRITE",
		mask,
	     regs[0], regs[1], regs[2], regs[3]);
}

static int acq200_ioctl_read_mbox(
	struct Acq200Device *device, struct MailboxControl *mbc)
{
	unsigned read_mask = MBC_FROM_MBXR(mbc);
	int ibox;
	
	for (ibox = MBOX_NUM; ibox-- > 0; ){
		if (((1<<ibox)&read_mask) != 0){
			mbc->mbxr[ibox] = MBOX_READ(device, ibox);
		}
	}

	if (acq200_debug_mbox){
		acq200_mbox_dbg(mbc, MBX_READ);
	}

	return 0;
}

static int _acq200_ioctl_write_mbox(
	struct Acq200Device *device, struct MailboxControl *mbc)
{
	unsigned write_mask = MBC_FROM_MBXW(mbc);
	int ibox;

	device->mbox_sync.response_received = 0;

	if (acq200_debug_mbox){
		acq200_mbox_dbg(mbc, !MBX_READ);
	}
	for (ibox = MBOX_NUM; ibox-- > 0; ){
		if (((1<<ibox)&write_mask) != 0){
			MBOX_WRITE(device, ibox, mbc->mbxw[ibox]);
		}
	}
	return 0;
}

#ifdef DANGEROUS
static int acq200_ioctl_write_mbox(
	struct Acq200Device *device, struct MailboxControl *mbc)
{
	if (down_interruptible(&device->mbox_sync.mutex)){
		return -ERESTARTSYS;
	}
	_acq200_ioctl_write_mbox(device, mbc);

	up(&device->mbox_sync.mutex);
	return 0;
}
#endif

static int acq200_ioctl_wbr_mbox(
	struct Acq200Device *device, struct MailboxControl *mbc)
/** write, block, read */
{
	long timeout = mbc->mbc_flags&MBC_FLAGS_TO;
	int rc = 0;

	if (down_interruptible(&device->mbox_sync.mutex)){
		return -ERESTARTSYS;
	}	

	_acq200_ioctl_write_mbox(device, mbc);

	if (device->mbox_sync.using_interrupt){
		if (timeout == MBC_FLAGS_TO_FOREVER){
			rc = wait_event_interruptible(
				device->mbox_sync.waitq, 
				RESPONSE_RECEIVED(device));
		}else{
			rc = wait_event_interruptible_timeout(
				device->mbox_sync.waitq, 
				RESPONSE_RECEIVED(device),
				timeout);
			if (rc == 0){
				rc = -ETIMEDOUT;
			}else if (rc > 0){
				rc = 0;		/* ticks left == OK */
			}
		}

		dbg(1, "back from wait rc:%d", rc);
	}else{
		rc = acq200_poll_response(device, timeout);
	}

	acq200_ioctl_read_mbox(device, mbc);
	up(&device->mbox_sync.mutex);

	return rc;
	
}
	

static int acq200_mbox_ioctl(
	struct file *filp,
	unsigned long arg,
	int (*action)(struct Acq200Device *, struct MailboxControl *))
{
	struct Acq200Device *device = DEV(filp);
	struct Acq200Path *path = PD(filp);
	struct MailboxControl *umbc = (struct MailboxControl*)arg;
	struct MailboxControl kmbc;
	int rc;

	dbg(1, "01 action %p", action);

	rc = copy_from_user(&kmbc, umbc, sizeof(kmbc));	
	if (rc != 0){
		err("returning EFAULT");
		return -EFAULT;
	}

	/** deferred action. */
	if ((kmbc.mbc_flags&MBC_FLAGS_ONRELEASE) != 0){
		memcpy(&path->mbc_on_release, &kmbc, sizeof(kmbc));
		return 0;
	}

	rc = action(device, &kmbc);

	rc = copy_to_user(umbc, &kmbc, sizeof(kmbc));
	if (rc != 0){
		err("returning EFAULT");
		return -EFAULT;
	}

	dbg(1, "99 ret %d", rc);
	return rc;
}


int acq200_ioctl (struct inode *inode, struct file *filp,
                        unsigned int cmd, unsigned long arg)
{
	dbg(1, "cmd 0x%08x", cmd);

	switch(cmd){

	case ACQ200_MBOX_IOR:
		return acq200_mbox_ioctl(filp, arg, acq200_ioctl_read_mbox);
#ifdef DANGEROUS
	case ACQ200_MBOX_IOW:
		return acq200_mbox_ioctl(filp, arg, acq200_ioctl_write_mbox);
#endif
	case ACQ200_MBOX_IOWR:
		return acq200_mbox_ioctl(filp, arg, acq200_ioctl_wbr_mbox);
	case ACQ32_IOG_PCIREGS_OFFSET: {
		struct Acq200Device *device = DEV(filp);
            	unsigned ba = device->csr.pa&(PAGE_SIZE-1);
            	ba += 0x10;
            	put_user(ba, (unsigned*)arg);         
            	return 0;
    	}
	case ACQ32_IOS_INTS_DISABLE:
		return acq200_intsDisable((unsigned)arg);	
	case ACQ32_IOS_INTS_ENABLE:
	        return acq200_intsEnable((unsigned)arg);
	default:			
		dbg(2, "wanted either 0x%08lx or 0x%08lx", 
		    (unsigned long)ACQ200_MBOX_IOR, 
		    (unsigned long)ACQ200_MBOX_IOWR);
		return -ENOTTY;
	}
}

int acq200_sendCommand(struct Acq200Device *device, u32 command)
/** send a command, single parameter */
{
	struct MailboxControl mbc = {
		.mbc_flags = DEFTO | MBC_TO_MBXW(1<<BP_MB_COMMAND) |
		MBC_TO_MBXR(0xf)
	};
	mbc.mbxw[BP_MB_COMMAND] = command | BP_CI_DONE | BP_CI_COMMAND;
	return acq200_ioctl_wbr_mbox(device, &mbc);
}

int acq200_sendCommandA3A4(
	struct Acq200Device *device, u32 command,
	u32 a3, u32 a4)
/** send a command with A3, A4 parameters */
{
	struct MailboxControl mbc = {
		.mbc_flags = DEFTO | 
		MBC_TO_MBXW((1<<BP_MB_COMMAND)|(1<<BP_MB_A3)|(1<<BP_MB_A4)) |
		MBC_TO_MBXR(0xf)
	};
	mbc.mbxw[BP_MB_COMMAND] = command | 
		BP_CI_DONE | BP_CI_COMMAND | BP_MB_A3 | BP_MB_A4;
	mbc.mbxw[BP_MB_A3] = a3;
	mbc.mbxw[BP_MB_A4] = a4;

	return acq200_ioctl_wbr_mbox(device, &mbc);
}

int acq200_sendQuery( 
    struct Acq200Device* device, 
    unsigned command, 
    unsigned* a1,
    unsigned* a2,
    unsigned* a3,
    unsigned* a4
    )
/** send a query, pick up answers for aX != NULL */
{
	struct MailboxControl mbc = { 
		.mbc_flags = DEFTO | 
		MBC_TO_MBXW(1<<BP_MB_COMMAND)|
		MBC_TO_MBXR(1<<BP_MB_COMMAND)
	};
	int rc;

	if (a3 != NULL_A3){
		mbc.mbc_flags |= MBC_TO_MBXR(1<<BP_MB_A3);
	}
	if (a4 != NULL_A4){
		mbc.mbc_flags |= MBC_TO_MBXR(1<<BP_MB_A4);
	}

	mbc.mbxw[BP_MB_COMMAND] = command | BP_CI_QUERY | BP_CI_DONE;
    
	rc = acq200_ioctl_wbr_mbox(device, &mbc);

	if (rc == 0){
		if (a1) *a1 = BP_GET_A1(mbc.mbxr[BP_MB_COMMAND]);
		if (a2) *a2 = BP_GET_A2(mbc.mbxr[BP_MB_COMMAND]);
		if (a3) *a3 = mbc.mbxr[BP_MB_A3];
		if (a4) *a4 = mbc.mbxr[BP_MB_A4];
	}

	return rc;
}



int acq200_handle_incoming_message(struct Acq200Device *device)
/* handle message - call from thread level */
{
	u32 mfa = readl(CSR(device, ACQ200_OQP));

	dbg(3, "mfa %08x", mfa );
	if (mfa == 0xffffffff || mfa == 0){
		return 0;
	}else{
		device->m_dpd.bad_mfa_count = 0;

		acq200_consume_i2o(device, mfa);
		return 1;
	}
}


irqreturn_t acq200_incoming_message_isr(struct Acq200Device *device)
/* handle message - call from interrupt level */
{
	u32 mfa = readl(CSR(device, ACQ200_OQP));

	dbg(1, "mfa %08x", mfa );
	/* get out of jail free trap */
	if (mfa == 0xffffffff || mfa == 0){
		err("%d bad mfa 0x%08x", device->idx, mfa);
		if (++device->m_dpd.bad_mfa_count > BAD_MFA_THRESHOLD){
			u32 mask = readl(CSR(device, ACQ200_OIMR));
			mask |= ACQ200_OISR_OPQI;
			writel(mask, CSR(device, ACQ200_OIMR));
			err(" bad mfa 0x%08x masking ints to get back in game",
			    mfa);
		}
		return IRQ_NONE;
	}else{
		device->m_dpd.bad_mfa_count = 0;
		acq200_consume_i2o(device, mfa);
		return IRQ_HANDLED;
	}
}


static irqreturn_t acq200_doorbell_isr(
	struct Acq200Device *device, u32 doorbell)
{
	dbg(1, "%s doorbell 0x%08x wakeup %p", 
	    device->ldev.drv_name, doorbell, &device->mbox_sync.waitq);

	device->mbox_sync.response_received = 1;
	dbg(1, "about to wake up");
	wake_up_interruptible(&device->mbox_sync.waitq);
	dbg(1, "wake done - do your worst");
	return IRQ_HANDLED;
}

static irqreturn_t acq200_isr(int irq, void* dev_id)
{
	static int bad_report;

	struct Acq200Device* device = (struct Acq200Device*)dev_id;
	u32 status = readl(CSR(device, ACQ200_OISR));
/* OIMR bits not interesting, and masked so not actually an interrupt */
#define IRQ_IGNORE (ACQ200_OISR_OM_0|ACQ200_OISR_OM_1)

	if ((status & ~IRQ_IGNORE) == 0){
		if ((status & IRQ_IGNORE) != 0){
			/** clear the status bits anyway */
			writel(status,CSR(device, IRQ_IGNORE));	
		}
		return IRQ_NONE;
	}else if (status&ACQ200_OISR_OPQI){ 
		dbg(1, "irq %d %s %s", irq, device->ldev.drv_name, "message");
		bad_report = 0;
		device->interrupt_count[0]++;
		return acq200_incoming_message_isr(device);
	}else if (status&ACQ200_OISR_ODI){
		u32 doorbell = readl(CSR(device, ACQ200_ODR));

		dbg(1, "irq %d %s %s 0x%08x",
		    irq, device->ldev.drv_name, "doorbell", doorbell);

		writel(doorbell, CSR(device, ACQ200_ODR));
		bad_report = 0;
		device->interrupt_count[1]++;
		return acq200_doorbell_isr(device, doorbell);
	}else{ 
		dbg(1, "irq %d %s %s", irq, device->ldev.drv_name, "bad");
		if ((bad_report&0xffff) == 0){
			err( "IRQ %d don't like this status 0x%08x", 
			     irq, status );
		}
		writel(status,CSR(device, ACQ200_OISR));
		bad_report++;
		device->interrupt_count[2]++;
		return IRQ_HANDLED;
	}
}

#define MYTURN 1

int acq200_onFirstClientOpenAB(struct Acq200Device *device)
{
	int rc = 0;

	device->i2o_backstop =  kthread_run(
		i2o_missed_int_checker, 
		device, 
		"acq200_i2o.%d", 
		device->idx);

	return rc;
}


int acq200_onLastClientReleaseAB(struct Acq200Device *device)
{
	int rc = 0;
	kthread_stop(device->i2o_backstop);
	return rc;
}
static int onOpen(struct Acq200Path *path)
{
	struct Acq200Device *device = path->device;
	int rc = 0;

	if (down_interruptible(&device->clients.mutex)){
		return -ERESTARTSYS;
	}
	if (++device->clients.count == 1){
		rc = acq200_onFirstClientOpenAB(device);
	}
	up(&device->clients.mutex);

	return rc;	
}

static int onRelease(struct Acq200Path *path)
{
	struct Acq200Device *device = path->device;
	int rc = 0;

	dbg(1, "into critical ...");
	if (down_interruptible(&device->clients.mutex)){
		err("possible corrupt mutex on interruption");
	}
	if (device->clients.count == 1){
		rc = acq200_onLastClientReleaseAB(device);
	}
	--device->clients.count;
	up(&device->clients.mutex);

	dbg(1, "clients.count %d", device->clients.count);
	return rc;	
}



int acq200_bridge_openAB(struct inode *inode, struct file *file)
{
	struct Acq200Device *device = DEV(file);
	struct Acq200Path *path = PD(file);

	if (device->AB[0].va == 0){
		struct device *dev = &device->pci_dev->dev;
		/* allocate buffer A, buffer B */
		alloc_max_buf(dev, &device->AB[0], DMA_FROM_DEVICE, 
				acq200_AB_dmabuf_max_order);
		alloc_max_buf(dev, &device->AB[1], DMA_FROM_DEVICE,
				acq200_AB_dmabuf_max_order);
	}

	copyDmaBuf2IoMapping(&path->bufferMappings[0], &device->AB[0]);
	copyDmaBuf2IoMapping(&path->bufferMappings[1], &device->AB[1]);
	onOpen(PD(file));
	return 0;
}
int acq200_bridge_releaseAB (
	struct inode *inode, struct file *file
	)
{
	onRelease(PD(file));
	return acq200_bridge_release(inode, file);
}

int acq200_ioctlAB (struct inode *inode, struct file *file,
                        unsigned int cmd, unsigned long arg)
{
	dbg(1, "cmd %d valid %lu", cmd, (unsigned long)ACQ200_BUFFER_AB_IOWR);

	switch(cmd){
	case ACQ200_BUFFER_AB_IOWR: {
		struct Acq200Path* path = PD(file);
		struct BufferAB_Ops ab_ops;
		int rc;

		if (down_interruptible(&path->device->buffer_AB_busy_mutex)){
			return -ERESTARTSYS;
		}else{
			path->has_AB_busy_mutex = 1;
		}
		COPY_FROM_USER(&ab_ops, (void*)arg, BUFFERAB_OPS_SZ);
		rc = acq200_bufferABaction(file, &ab_ops);

		up(&path->device->buffer_AB_busy_mutex);
		path->has_AB_busy_mutex = 0;

		if (rc == 0){
			COPY_TO_USER((void*)arg, &ab_ops, BUFFERAB_OPS_SZ);
		}
		return rc;
	}
	default:
		return -ENOTTY;
	}
}


int acq200_open (struct inode *inode, struct file *file)
{
	static struct file_operations acq200_fops = {
		.read = acq200_read,
		.write = acq200_write,
		.release = acq200_release,
		.ioctl = acq200_ioctl,
		.mmap = acq200_mmap_host
	};
	static struct file_operations acq200_llhost_fops = {
		.read = acq200_read,
		.write = acq200_write,
		.release = acq200_release,
		.ioctl = acq200_ioctl,
		.mmap = acq200_mmap_llhost
	};
	static struct file_operations acq200_mbox_fops = {
		.read = acq200_read,
		.write = acq200_write,
		.release = acq200_release,
		.ioctl = acq200_ioctl,
		.mmap = acq200_mmap_mbox
	};
	static struct file_operations acq200_bridge_fops = {
		.read = acq200_bridge_read,
		.write = acq200_bridge_write,
		.release = acq200_bridge_release
	};
	static struct file_operations acq200_bridge_AB_fops = {
		.open = acq200_bridge_openAB,
		.read = acq200_bridge_readAB,
		.ioctl = acq200_ioctlAB,
		.mmap = acq200_mmapAB,
		.write = acq200_bridge_write,
		.release = acq200_bridge_releaseAB
	};
#define RETURN_OPEN_NEWFOPS(newfops)			\
	file->f_op = &newfops;				\
	goto return_open_take_busy;

#define RETURN_OPEN_NEWFOPS_NOSEM(newfops)		\
	file->f_op = &newfops;				\
	goto return_open;

	struct Acq200Device *device = acq200_lookup_device(inode);
	struct Acq200Path *path;

	dbg(1, "01");

	if (device == 0){
		return -ENODEV;
	}

	path = acq200_createPath(inode, device);
	SET_PATH(file, path);

	if (DEVRAW(path->minor)){
		RETURN_OPEN_NEWFOPS_NOSEM(acq200_mbox_fops);
	}else if (BIGDUMPDEV(path->minor)){
		RETURN_OPEN_NEWFOPS_NOSEM(acq200_fops);
	}else if (LLHOSTDEV(path->minor)){
		RETURN_OPEN_NEWFOPS_NOSEM(acq200_llhost_fops);
	}else if (IS_CHANNEL_DEV(path->minor)){
		if (path->minor > 96){
			file->f_op = &acq200_bridge_AB_fops;
			goto return_open;
		}else{
			RETURN_OPEN_NEWFOPS(acq200_bridge_fops);
		}
	}else if (IS_FILE(path->minor)){
		RETURN_OPEN_NEWFOPS(acq200_bridge_fops);
	}else if (IS_RSH(path->minor)){
		RETURN_OPEN_NEWFOPS(acq200_bridge_fops);
	}else{
		/* placeholder */
		RETURN_OPEN_NEWFOPS(acq200_fops);
	}

	return -ENODEV;

return_open_take_busy:
	if (down_interruptible(&device->buffer_AB_busy_mutex)){
		return -ERESTARTSYS;
	}else{
		dbg(1, "down");
		path->has_AB_busy_mutex = 1;
	}

return_open:
	if (file->f_op->open != 0){
		return file->f_op->open(inode, file);
	}else{
		return 0;
	}
}

int acq200_release (struct inode *inode, struct file *file)
{
	struct Acq200Path* path = PD(file);

	if (path->has_AB_busy_mutex){
		dbg(1, "up");
		up(&path->device->buffer_AB_busy_mutex);
	}
	if ((path->mbc_on_release.mbc_flags&MBC_FLAGS_ONRELEASE) != 0){
		path->mbc_on_release.mbc_flags&= ~MBC_FLAGS_ONRELEASE;
		acq200_ioctl_wbr_mbox(DEV(file), &path->mbc_on_release);
	}
	acq200_deletePath(path);
	dbg(1, "01");
	return 0;
}


static int __devinit
acq200_device_init(struct Acq200Device *device)
{
	static struct file_operations acq200_default_fops = {
		.open = acq200_open,
		.release = acq200_release,
		.ioctl = acq200_ioctl,
	};
	int rc;


	rc = pci_enable_device(device->pci_dev);
	if (rc != 0){
		err("pci_enable_device FAILED returning %d", rc);
		return rc;
	}

	slots[device->idx] = acq200_get_cpci_slot(device->pci_dev);
	
	rc = register_chrdev(device->ldev.major = 0, 
			     device->ldev.drv_name, 
			     &acq200_default_fops);

	if (rc < 0){
		err( "can't get major %d", device->ldev.major );
		return rc;
	}else{
		device->ldev.major = rc;
		info( "device major set %d\n", device->ldev.major );
	}

/*
 * if interrupts are requested globally, AND the board is wanting an interrupt,
 * ask Linux for the use thereof
 */
	if (acq200_use_interrupts != 0 && device->pci_dev->irq != 0){
		rc = request_irq( 
			device->pci_dev->irq, 
			ACQ200_ISR, 
			IRQF_SHARED,
			device->ldev.drv_name, 
			device
		);

		if (rc != 0){
			err("request_irq %d FAILED", device->pci_dev->irq);
			return rc;
		}
	}

	if (device->mbox_sync.using_interrupt){
		rc = acq200_sendCommand(device, 
			   BP_SET_FUNCODE(BP_FC_SET_INTERRUPTS)+BP_SET_A1(3));
		if (rc != 0){
			err("BP_FC_SET_INTERRUPTS failed %d", rc);
			return rc;
		}
	}

	if (device->completed_onetime_setups == 0){
		unsigned funcode = 
			BP_SET_FUNCODE(BP_FC_SET_HOST_DMABUF)|
			BP_SET_A1(NMAPPINGS);
		rc = acq200_sendCommandA3A4(device,
			funcode,
			(u32)device->dma_buf.pa, 
			dma_buf_len(&device->dma_buf));
		device->completed_onetime_setups = 1;

		if (rc != 0){	
			err("BP_FC_SET_HOST_DMABUF failed %d", rc);
			return rc;
		}

		rc = acq200_getAvailableChannels(device);
	}


	acq200_create_sysfs_device(&device->pci_dev->dev);

	device->class_dev = CLASS_DEVICE_CREATE(
		acq200_device_class,			/* cls */
		NULL,					/* cls_parent */
		device->idx,				/* "devt" */
		&device->pci_dev->dev,			/* device */
		"acq200.%d", device->idx);		 /* fmt, idx */

	acq200_create_sysfs_class(device);

	spin_lock_init(&device->bkl.lock);
	return rc;	
}


static void acq200_set_dev_type(
	struct Acq200Device* device, const struct pci_device_id *ent)
{
	if (IS_ACQ196(ent)){
		device->card_type = DEV_TYPE_ACQ196;
		sprintf(device->model_name, "%s", "acq196");
	}else{
		device->card_type = DEV_TYPE_ACQ216;
		sprintf(device->model_name, "%s", "acq216");
	}
}


static int __devinit
acq200_probe(struct pci_dev *dev, const struct pci_device_id *ent)
{
	struct Acq200Device* device = acq200_device_create(dev);

	acq200_set_dev_type(device, ent);
	return acq200_device_init(device);
}


static void acq200_remove (struct pci_dev *dev)
{
	struct Acq200Device *device = acq200_lookupDevice(&dev->dev);
	if (device != 0){

		int used_interrupts = device->mbox_sync.using_interrupt != 0;

		if (used_interrupts){
			free_irq(device->pci_dev->irq, device);
			device->mbox_sync.using_interrupt = 0;
		}

		dbg(1, "free_irq %s", used_interrupts? "YES": "NO");

		acq200_remove_sysfs_class(device);
		acq200_remove_sysfs_device(&device->pci_dev->dev);
		device_destroy(acq200_device_class, device->idx);

		unregister_chrdev(
				device->ldev.major, device->ldev.drv_name);
		acq200_device_free(device);
	}
}


/*
 *
 * { Vendor ID, Device ID, SubVendor ID, SubDevice ID,
 *   Class, Class Mask, String Index }
 */
static struct pci_device_id acq200_pci_tbl[] __devinitdata = {
	{PCI_VENDOR_ID_HINT, 0x0029, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_INTEL, 0x0318, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_INTEL, 0x0319, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ }
};
static struct pci_driver acq200_driver = {
	.name     = acq200_driver_name,
	.id_table = acq200_pci_tbl,
	.probe    = acq200_probe,
	.remove   = __devexit_p(acq200_remove),
#ifdef PGMCOMOUT
	/* Power Managment Hooks */
#ifdef CONFIG_PM
	.suspend  = acq200_suspend,
	.resume   = acq200_resume
#endif
#endif
};




static int __init acq200_init( void )
{
	int rc;

	acq200_device_class = class_create(THIS_MODULE, "acqX00");
	rc = pci_register_driver(&acq200_driver);
	dbg(1, "pci_register_driver() returned %d", rc );
	return 0;
}

int acq200_init_module(void)
{
	info("%s\n%s\n%s\n%s", 
	     acq200_driver_name, acq200__driver_string,
	     acq200__driver_version, acq200__copyright);
	acq200_init();
	return 0;
}

void acq200_exit_module(void)
{
	if (!idx) return;
	pci_unregister_driver(&acq200_driver);
	class_destroy(acq200_device_class);
}


module_init(acq200_init_module);
module_exit(acq200_exit_module);

MODULE_DEVICE_TABLE(pci, acq200_pci_tbl);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Peter.Milne@d-tacq.com");
MODULE_DESCRIPTION("Driver for ACQ200 BRIDGE");


