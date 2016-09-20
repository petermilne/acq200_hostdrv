/* ------------------------------------------------------------------------- */
/* acq200.h header for driver toplevel for acq200 peripheral                 */
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

/** @file acq200.h <b>internal header file for acq200 Linux PCI host driver</b>.
 */


#ifndef __ACQ200_H__
#define __ACQ200_H__

#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/version.h>

#include <linux/spinlock.h>

#include "streambuf.h"

#include "acq200_ioctl.h"

/* kernel compatibility ugliness */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))

#define CLASS_DEVICE		device
#define CLASS_DEVICE_CREATE	device_create
#define CLASS_DEVICE_ATTR	DEVICE_ATTR
#define CLASS_DEVICE_CREATE_FILE DEVICE_CREATE_FILE
#else
#define ORIGINAL_CLASS_DEVICE_INTERFACE	1
#define CLASS_DEVICE		class_device
#define CLASS_DEVICE_CREATE	class_device_create

#endif



#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,18)
#ifdef EXPORT_SYMTAB		/* main module */
#pragma message "building for kernel 2.6.18"

static irqreturn_t acq200_isr(int irq, void* dev_id);
static irqreturn_t __acq200_isr(
	int irq, void *data, struct pt_regs *not_used)
{
	return acq200_isr(irq, data);
}

#define ACQ200_ISR	__acq200_isr

#endif	/* EXPORT_SYMTAB */

#else	/* KERNEL_VERSION(2,6,18) */
#define ACQ200_ISR	acq200_isr
#endif	/* KERNEL_VERSION(2,6,18) */
/* kernel compatibility ugliness ends */

#define NMAPPINGS 4

/* @@todo - select on subvendor as well as vendor ... */
#define IS_ACQ196(ent) ((ent)->vendor == PCI_VENDOR_ID_INTEL)

#define DEV_TYPE_ACQ32  0x0100
#define DEV_TYPE_ACQ16  0x0200
#define DEV_TYPE_ACQ196 0x1000
#define DEV_TYPE_ACQ164 0x1100
#define DEV_TYPE_WAV232 0x2000
#define DEV_TYPE_ACQ216 0x2100



struct Acq200Path;

struct MESSAGE;

#define RCH_KB_POOL	1


struct RchReturn {
	int valid;
	unsigned start_id;
	int nbytes;
	struct IoMapping *kbuf;    /* optional .. is there a kbuf here */
	unsigned flags;
};

struct ReturnCommandHandler {
	void (*handler)( 
		struct ReturnCommandHandler* this, 
		struct MESSAGE* messsage );        

	struct Acq200Path* path;                        // active path
	struct Acq200Device *device;
	short command_id;                              // command id
	struct RchReturn rchReturn;
	spinlock_t *lock;

	struct list_head list;	
};


#define MK_DMABUF_OFFSET(kbuf, device) \
	(kbuf->pa & PCI_WINDOW_MASK(&device->dma_buf))

#define USE_POOL 0

struct DmaBuf {
	int order;
	void *va;
	unsigned pa;
};
#define ORDER2LEN(order) ((1<<(order)) * PAGE_SIZE)

#define KNAME	32

/** device descriptor, one per device */
struct Acq200Device {
	int idx;				/**< card index in system. */

	struct pci_dev *pci_dev;		/**< linux pci generic.    */
	struct CLASS_DEVICE *class_dev;		/**< class generic.	   */
	int card_type;
	char model_name[KNAME];
	
	struct LogicalDev {
		int major;
		char drv_name[KNAME];
	}
		ldev;

	struct DmaBuf llhost_dma_buf;		/** use for LLC */
	struct DmaBuf dma_buf;
	struct DmaBuf AB[2];
	struct BigBufPool* pool;

	struct IoMapping {
		u32* va;
		unsigned long pa;
		int len;
		char name[KNAME];
	}
		csr, ram, rom;

	char imask[NR_IRQS/4+1];	/* string of hex coded chars. RH=1 */
	unsigned char imask_a[NR_IRQS/4+1]; /* active (disabled) mask, coded as nibbles */

	struct MailboxSync {
		long using_interrupt;
		struct semaphore mutex;
		int response_received;

		wait_queue_head_t waitq;
	} mbox_sync;

	struct semaphore buffer_AB_busy_mutex;
	struct Clients {
		struct semaphore mutex;
		int count;
	} clients;

	struct CHANNEL_INFO {
		unsigned nchannels;        // #channels available
		/** @todo vmax10, vmin10 are deprecated .. */
		short vmax10;              // vmax (volts*10)
		short vmin10;              // vmin (volts*10)
	}
		input_channels, output_channels;

	int completed_onetime_setups;
	int heartbeat_detected;
	int interrupt_count[4];

	struct DevicePrivateData {
		int i2o_packets_incoming;
		int i2o_packets_returned;
		int i2o_packets_discarded;
		void* i2o_last_in;
		
		int i2o_packets_outgoing;       // # command packets sent
		short i2o_last_id_out;              // last id code

		struct rch {
			spinlock_t lock;
			struct list_head list;
		} rch;

		int streaming_enabled;

		struct tasklet_struct streaming_isr_tasklet;

		unsigned aux_mfa;
		wait_queue_head_t aux_waitq; 
		int aux_task_waiting;

		unsigned bad_mfa_count;
	} m_dpd;

	struct StreamBuf streambuf;
	struct task_struct *i2o_backstop;

	/* locks local CPU tight big, ugly and bad.*/
	struct BKL {
		spinlock_t lock;
		unsigned long flags;
		int is_locked;
	} bkl;
};

#define BAD_MFA_THRESHOLD 100

/*
 * MU shared defs for driver comms
 */

#define ACQ200_MAILBOX_0         0x10
#define ACQ200_ODR               0x2c
#define ACQ200_OISR              0x30
#define ACQ200_OIMR              0x34
#define ACQ200_IQP               0x40
#define ACQ200_OQP               0x44

#define ACQ200_OISR_OM_0         0x01  /* not used */
#define ACQ200_OISR_OM_1         0x02  /* not used */
#define ACQ200_OISR_ODI          0x04  /* we use this one */
#define ACQ200_OISR_OPQI         0x08  
#define ACQ200_OISR_PCI_INTA     0x10  /* not obvious ?? */

#define PCI_I2O_INVALID_MFA         0xffffffff

#define CSR( dev, byte_off )    ((dev)->csr.va+(byte_off)/4)
#define RESPONSE_RECEIVED(dev) ((dev)->mbox_sync.response_received != 0)

/** path descriptor, one per open path */
struct Acq200Path {
	unsigned minor;                 /**  customisation for this node. */
	struct Acq200Device* device;
	struct DmaBuf dma_buf;
	struct DmaBuf llhost_dma_buf;
	int last_direction_hack;
	wait_queue_head_t return_waitq;
	struct ReturnCommandHandler* rchAB[2];
	struct IoMapping bufferMappings[2];
	int ab_next;				/* BUFFERA, BUFFERB */
	unsigned long sid;
	int has_AB_busy_mutex;
	struct MailboxControl mbc_on_release;
	char scratch[2048];
};

#define PATH_HEAD (offsetof(struct Acq200Path, scratch))

/** last_direction_hack args */
#define LDH_OUTBOUND 1
#define LDH_INBOUND  -1
#define LDH_NONE 0


#define ACQ200_ON_OPEN(inode, file)\
        do { \
                if ((PD(file)=acq200_makePathDescriptor(inode->i_rdev)) == 0)\
                        return -ENODEV;\
                else \
                         DG(file)->bridge_dev.open_count++; \
	 } while(0)

#define ACQ200_ON_CLOSE(file) \
        do { \
                if (PD(file) != 0){ \
                         acq200_freePathDescriptor(PD(file));\
                         DG(file)->bridge_dev.open_count--;\
                         PD(file) = 0;\
                } \
                return 0; \
         } while(0)

#define MBOX_OFFSET 0x10
#define MBOX_OFFSETL (MBOX_OFFSET/sizeof(u32))


#define MBOX(device, ibox) (device->csr.va[MBOX_OFFSETL+(ibox)])

#define MBOX_WRITE(device, ibox, value)		\
	iowrite32((value), &MBOX(device, ibox))
#define MBOX_READ(device, ibox) ioread32(&MBOX(device, ibox))


#define PD(file)	     ((struct Acq200Path*)file->private_data)
#define DEV(file)	     (PD(file)->device)
#define SET_PATH(file, path) (file->private_data = (path))

#define LBUF(file) (PD(file)->scratch)

static inline int dma_buf_len(struct DmaBuf *dma_buf) 
{
	if (dma_buf->va != 0) {
		return (1 << dma_buf->order) * PAGE_SIZE;
	} else {
		return 0;
	}
}



struct Acq200Device* acq200_lookupDevice(struct device *dev);
struct Acq200Device* acq200_lookupDeviceFromClass(struct CLASS_DEVICE *dev);

void acq200_create_sysfs_device(struct device *dev);
void acq200_remove_sysfs_device(struct device *dev);
void acq200_create_sysfs_class(struct Acq200Device *adev);
void acq200_remove_sysfs_class(struct Acq200Device *adev);

int acq200_mmap_host(struct file* file, struct vm_area_struct* vma);
int acq200_mmap_llhost(struct file* file, struct vm_area_struct* vma);

/**
 * mmap the host buffer.
 */
int acq200_mmapAB(struct file* file, struct vm_area_struct* vma);

struct BufferAB_Ops;
int acq200_bufferABaction(struct file *file, struct BufferAB_Ops *ops);


int acq200_mmap_mbox(struct file* file, struct vm_area_struct* vma);
/**
 *  provide user mapping to mailboxes on target
 */

int acq200_map_pci_memory(struct Acq200Device* device);
/**
 *  map remote regs area on device load
 */

void acq200_unmap_pci_memory( struct Acq200Device* device );
/**
 * unmap remote regs are on device unload
 */

int acq200_intsMask(struct Acq200Device* device);
/**< disable interrupts defined by set bits in imask, a string of hex chars */

int acq200_intsDisable( unsigned irqs );
/**< disable interrupts defined by clear bits in irqs */

int acq200_intsEnable( unsigned irqs );
/**< enable interrupts defined by set bits in irqs */

#define STATUS dbg(1, "STATUS line %d", __LINE__ )



int acq200_getHeartbeat( struct Acq200Device* device );
// return heartbeat

int acq200_sendCommand(struct Acq200Device *device, u32 command);
/** send a command, single parameter */

int acq200_sendCommandA3A4(
	struct Acq200Device *device, u32 command, u32 a3, u32 a4);
/** send a command with A3, A4 parameters */


#define NULL_A3	NULL
#define NULL_A4 NULL

int acq200_sendQuery( 
	struct Acq200Device* device, 
	unsigned command, 
	unsigned* a1,
	unsigned* a2,
	unsigned* a3,
	unsigned* a4
	);
/** send a query, pick up answers for aX != NULL */


int acq200_getAvailableChannels(struct Acq200Device* device);
/**< update available input, output channels from device */



void acq200_consume_i2o(struct Acq200Device* device, u32 mfa );

#define TIMESTAMP(a, b, c)
#define ACQ200_PCIWINSZ  0x01000000
#define ACQ200_PCIWINMSK (ACQ200_PCIWINSZ-1)

void acq200_streaming_isr_bh(unsigned long clidata);
ssize_t acq200_bridge_write(
	struct file *file, const char *buf, size_t count, loff_t * posp);
ssize_t acq200_bridge_read (
	struct file* file, char* buf, size_t count, loff_t* posp);
ssize_t acq200_bridge_readAB (
	struct file* file, char* buf, size_t count, loff_t* posp);
int acq200_bridge_release (struct inode *inode, struct file *file);
int acq200_release (struct inode *inode, struct file *file);

#define ACQ32_BB_POOL_SIZE 0x100000
#define ACQ32_BB_POOL_ERROR 0xffffffff

void acq200_pool_create(struct Acq200Device *device);
void acq200_pool_delete(struct Acq200Device *device);

struct IoMapping *acq200_pool_alloc_mapping(
	struct Acq200Device *device, int direction);

void acq200_pool_free_mapping(
	struct Acq200Device *device, struct IoMapping *map);

struct ReturnCommandHandler* 
	acq200_pool_alloc_rch(struct Acq200Device *device);

void acq200_pool_free_rch(
	struct Acq200Device *device,
	struct ReturnCommandHandler* rch);
	
int acq200_get_cpci_slot(struct pci_dev * pci_dev);

#ifdef DEBUG_MALLOC
#define KMALLOC( size, flags ) dbg_kmalloc( size, flags, __FILE__, __LINE__ )
#define KFREE( addr )          __dbg_kfree( addr, __FILE__, __LINE__ )
#else
#define KMALLOC( size,flags )  kmalloc( size, flags )
#define KFREE( addr )          kfree( addr )
#endif


#define PCI_WINDOW_MASK(device) (dma_buf_len(device) -1)

extern void acq200_device_create_file(
	struct device * dev, struct device_attribute * attr,
	const char *file, int line);

#define DEVICE_CREATE_FILE(dev, attr) \
	acq200_device_create_file(dev, attr, __FILE__, __LINE__)


#define COPY_TO_USER(src, dest, len) \
	if (copy_to_user(src, dest, len)) { return -EFAULT; }

#define COPY_FROM_USER(src, dest, len) \
	if (copy_from_user(src, dest, len)) { return -EFAULT; }


extern int acq200_stub_copy_to_user;

static inline void copyDmaBuf2IoMapping(
	struct IoMapping *mapping, struct DmaBuf *dma_buf)
{
	mapping->va = dma_buf->va;
	mapping->pa = dma_buf->pa;
	mapping->len = dma_buf_len(dma_buf);
}

extern int i2o_missed_int_checker(void *arg);

extern int acq200_handle_incoming_message(struct Acq200Device *device);

#define BKL_LOCK	1
#define BKL_UNLOCK	0
#define BKL_QUERY	-1

extern int acq200_bkl(struct Acq200Device* device, int bkl_action);

#endif /* __ACQ200_H__ */

