/* ------------------------------------------------------------------------- */
/* acq200if.c interface layer for acq200 peripheral                          */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2006 Peter Milne, D-TACQ Solutions Ltd
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

/** @file acq200_if.c interface layer for acq200 peripheral. */

#include <linux/ctype.h>
#include <linux/kernel.h>
//#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/time.h>
#include <linux/interrupt.h>       
#include <linux/init.h>
#include <linux/timex.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>

#include <linux/proc_fs.h>

#include <asm/uaccess.h>  /* VERIFY_READ|WRITE */

#include <asm/apic.h>

#include "acq200_debug.h"
#include "acq200.h"
#include "acq200_minors.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,6,35))
void disable_local_APIC(void)
{
	err(" function not supported by this kernel");
}
#endif

static ssize_t show_heartbeat(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct Acq200Device *device = acq200_lookupDevice(dev);

	if (device != 0){
		return sprintf(buf, "%d\n", acq200_getHeartbeat(device));
	}else{
		return -ENODEV;
	}
}

static DEVICE_ATTR(heartbeat, S_IRUGO, show_heartbeat, 0);


/* from getBoards.pm ..
	if ( $refbrec->{fun} >= 72 && $refbrec->{fun} <= 120 ){
# works for CPV5350 in Schroff backplane
	    $refbrec->{lognum} = 9-($refbrec->{fun}/8-8);
#	    print "getSlotBoardNum() returns $refbrec->{lognum}\n";
	}else{
	    die "Slot addressing scheme failed";
	}
    }
*/

int acq200_get_cpci_slot(struct pci_dev * pci_dev)
{
	u32 devfn =  pci_dev->devfn;
	int slot = -1;

	if (devfn >= 72 && devfn <= 120){
		slot = 9 - (devfn/8 -8);			
	}
	return slot;	
}
static ssize_t show_cpci_slot(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct Acq200Device *device = acq200_lookupDevice(dev);

	if (device != 0){		
		return sprintf(buf, "%d\n", 
			acq200_get_cpci_slot(device->pci_dev));
	}else{
		return -ENODEV;
	}
}

static DEVICE_ATTR(cpci_slot, S_IRUGO, show_cpci_slot, 0);

static ssize_t show_buffer_AB_busy_mutex(
		struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct Acq200Device *device = acq200_lookupDevice(dev);

	if (device != 0){
		int rc = down_trylock(&device->buffer_AB_busy_mutex);
		if (rc == 0){
			up(&device->buffer_AB_busy_mutex);
		}
		return sprintf(buf, "%d\n", rc);
	}else{
		return -ENODEV;
	}
}

static DEVICE_ATTR(buffer_AB_busy_mutex, S_IRUGO, show_buffer_AB_busy_mutex, 0);

static ssize_t show_interrupt_count(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct Acq200Device *device = acq200_lookupDevice(dev);

	if (device != 0){
		return sprintf(buf, "%d %d %d\n", 
			       device->interrupt_count[0],
			       device->interrupt_count[1],
			       device->interrupt_count[2]
			);
	}else{
		return -ENODEV;
	}
}

static DEVICE_ATTR(interrupt_count, S_IRUGO, show_interrupt_count, 0);

static ssize_t show_host_pa(
	struct device *dev, 
	struct device_attribute *attr,
	char *buf)
{
	struct Acq200Device *device = acq200_lookupDevice(dev);

	if (device != 0){
		return sprintf(buf, "0x%08x\n", device->dma_buf.pa);
	}else{
		return -ENODEV;
	}
}

static DEVICE_ATTR(host_pa, S_IRUGO, show_host_pa, 0);

static ssize_t show_host_len(
	struct device *dev, 
	struct device_attribute *attr,
	char *buf)
{
	struct Acq200Device *device = acq200_lookupDevice(dev);

	if (device != 0){
		return sprintf(buf, "%d\n", dma_buf_len(&device->dma_buf));
	}else{
		return -ENODEV;
	}
}

static DEVICE_ATTR(host_len, S_IRUGO, show_host_len, 0);

static ssize_t show_llhost_pa(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct Acq200Device *device = acq200_lookupDevice(dev);

	if (device != 0){
		return sprintf(buf, "0x%08x\n", device->llhost_dma_buf.pa);
	}else{
		return -ENODEV;
	}
}

static DEVICE_ATTR(llhost_pa, S_IRUGO, show_llhost_pa, 0);

static ssize_t show_llhost_len(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct Acq200Device *device = acq200_lookupDevice(dev);

	if (device != 0){
		return sprintf(buf, "%d\n", dma_buf_len(&device->llhost_dma_buf));
	}else{
		return -ENODEV;
	}
}

static DEVICE_ATTR(llhost_len, S_IRUGO, show_llhost_len, 0);


static ssize_t show_available_input_channels(
	struct device *dev, 
	struct device_attribute *attr,
	char *buf)
{
	struct Acq200Device *device = acq200_lookupDevice(dev);

	if (device != 0){
		return sprintf(buf, "%d\n", device->input_channels.nchannels);
	}else{
		return -ENODEV;
	}
}

static DEVICE_ATTR(available_input_channels, S_IRUGO, 
	show_available_input_channels, 0);


static ssize_t show_available_output_channels(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct Acq200Device *device = acq200_lookupDevice(dev);

	if (device != 0){
		return sprintf(buf, "%d\n", device->output_channels.nchannels);
	}else{
		return -ENODEV;
	}
}

static DEVICE_ATTR(available_output_channels, S_IRUGO,
		   show_available_output_channels, 0);


static ssize_t dumpDmaBuf(
	struct DmaBuf *dma_buf, char *buf, int maxbuf, const char* id)
{
	return snprintf(buf, maxbuf, "%10s:%d %p 0x%08x len %d\n",
			id,
			dma_buf->order, dma_buf->va, dma_buf->pa,
			dma_buf_len(dma_buf));
}

static ssize_t show_buffers(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct Acq200Device *device = acq200_lookupDevice(dev);
	int cc = dumpDmaBuf(&device->dma_buf, buf, 80, "dma_buf");	
	if (device->AB[0].va){
		cc += dumpDmaBuf(&device->AB[0], buf+cc, 80, "buffer A");
	}
	if (device->AB[1].va){
		cc += dumpDmaBuf(&device->AB[1], buf+cc, 80, "buffer B");
	}
	return cc;
}


static DEVICE_ATTR(buffers, S_IRUGO, show_buffers, 0);

void acq200_device_create_file(
	struct device * dev, struct device_attribute * attr,
	const char *file, int line)
{
	if (device_create_file(dev, attr)){
		err("%s:%d device_create_file", file, line);
	}
}


void acq200_create_sysfs_device(struct device *dev)
{
	dbg(1, "01");
	DEVICE_CREATE_FILE(dev, &dev_attr_host_pa);
	DEVICE_CREATE_FILE(dev, &dev_attr_host_len);
	DEVICE_CREATE_FILE(dev, &dev_attr_llhost_pa);
	DEVICE_CREATE_FILE(dev, &dev_attr_llhost_len);
	DEVICE_CREATE_FILE(dev, &dev_attr_heartbeat);
	DEVICE_CREATE_FILE(dev, &dev_attr_cpci_slot);
	DEVICE_CREATE_FILE(dev, &dev_attr_buffer_AB_busy_mutex);
	DEVICE_CREATE_FILE(dev, &dev_attr_interrupt_count);

	DEVICE_CREATE_FILE(dev, &dev_attr_available_input_channels);
	DEVICE_CREATE_FILE(dev, &dev_attr_available_output_channels);
	DEVICE_CREATE_FILE(dev, &dev_attr_buffers);
	dbg(1, "99");
}

void acq200_remove_sysfs_device(struct device *dev)
{
	device_remove_file(dev, &dev_attr_host_pa);
	device_remove_file(dev, &dev_attr_host_len);
	device_remove_file(dev, &dev_attr_llhost_pa);
	device_remove_file(dev, &dev_attr_llhost_len);
	device_remove_file(dev, &dev_attr_heartbeat);
	device_remove_file(dev, &dev_attr_cpci_slot);
	device_remove_file(dev, &dev_attr_buffer_AB_busy_mutex);
	device_remove_file(dev, &dev_attr_interrupt_count);

	device_remove_file(dev, &dev_attr_available_input_channels);
	device_remove_file(dev, &dev_attr_available_output_channels);
	device_remove_file(dev, &dev_attr_buffers);
}



static ssize_t show_imask(
	struct CLASS_DEVICE *dev, 
#ifndef ORIGINAL_CLASS_DEVICE_INTERFACE
	struct device_attribute *attr, 
#endif
	char* buf)
{
	struct Acq200Device *device = acq200_lookupDeviceFromClass(dev);

	if (device != 0){
		return sprintf(buf, "%s\n", device->imask);
	}else{
		return -ENODEV;
	}
}

static ssize_t store_imask(
	struct CLASS_DEVICE *dev, 
#ifndef ORIGINAL_CLASS_DEVICE_INTERFACE
	struct device_attribute *attr, 
#endif
	const char *buf, size_t count)
{
	struct Acq200Device *device = acq200_lookupDeviceFromClass(dev);

	if (device != 0){
		if (strncmp(buf, "LAPIC", 5) == 0){
			disable_local_APIC();
		}else if (strncmp(buf, "LOC", 3) == 0){
			acq200_bkl(device, BKL_LOCK);
		}else if (strncmp(buf, "000", 3) == 0 &&
				acq200_bkl(device, BKL_QUERY)){
			acq200_bkl(device, BKL_UNLOCK);
		}else{
			int ic;
			strncpy(device->imask, buf, NR_IRQS/4);
			for (ic = strlen(device->imask); ic--; ){
				if (!isxdigit(device->imask[ic])){
					device->imask[ic] = '\0';
				}
			}
			acq200_intsMask(device);
		}
		return strlen(buf);
	}else{
		return -ENODEV;
	}
}

static CLASS_DEVICE_ATTR(imask, S_IRUGO|S_IWUSR, show_imask, store_imask);


static ssize_t show_dev(
	struct CLASS_DEVICE *dev, 
#ifndef ORIGINAL_CLASS_DEVICE_INTERFACE
	struct device_attribute *attr, 
#endif
	char *buf)
/* major:minor output for udev. minor is a dummy (we use all of them!) */
{
	struct Acq200Device *device = acq200_lookupDeviceFromClass(dev);

	if (device){
		return sprintf(buf, "%d:%d\n", device->ldev.major, 0);
	}else{
		return -ENODEV;
	}
}


static CLASS_DEVICE_ATTR(dev, S_IRUGO, show_dev, 0);

#ifdef ORIGINAL_CLASS_DEVICE_INTERFACE
void acq200_class_device_create_file(
        struct class_device * dev, struct class_device_attribute * attr,
        const char *file, int line)
{
        if (class_device_create_file(dev, attr)){
                err("%s:%d device_create_file", file, line);
        }
}

#define CLASS_DEVICE_CREATE_FILE(dev, attr) \
        acq200_class_device_create_file(dev, attr, __FILE__, __LINE__)

void acq200_create_sysfs_class(struct Acq200Device *adev)
{
        dbg(1, "01");
        CLASS_DEVICE_CREATE_FILE(adev->class_dev, &class_device_attr_imask);
        CLASS_DEVICE_CREATE_FILE(adev->class_dev, &class_device_attr_dev);
        dbg(1, "9");
}

void acq200_remove_sysfs_class(struct Acq200Device *adev)
{
        class_device_remove_file(adev->class_dev, &class_device_attr_imask);
        class_device_remove_file(adev->class_dev, &class_device_attr_dev);
}


#else
void acq200_create_sysfs_class(struct Acq200Device  *adev)
{
	int rc;
	dbg(1, "01");
	DEVICE_CREATE_FILE(adev->class_dev, &dev_attr_imask);
	DEVICE_CREATE_FILE(adev->class_dev, &dev_attr_dev);

#ifndef ORIGINAL_CLASS_DEVICE_INTERFACE
	/* create a symlink for the device */
	rc = sysfs_create_link(
		&adev->class_dev->kobj, &adev->pci_dev->dev.kobj, "device");
#endif
	if (rc) {
		err("failed to create symlink %s\n", "device");
	}	


	dbg(1, "9");
}

void acq200_remove_sysfs_class(struct Acq200Device *adev)
{
	device_remove_file(adev->class_dev, &dev_attr_imask);
	device_remove_file(adev->class_dev, &dev_attr_dev);
}

#endif


