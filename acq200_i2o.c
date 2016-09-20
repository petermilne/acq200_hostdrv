/* ------------------------------------------------------------------------- */
/* acq200_i2o.c acq200 block data handling		                     */
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


/** @file acq200_i2o.c i2o message passing implementation. */

struct task_struct;

#include <linux/kernel.h>
//#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/timex.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>


#include <linux/proc_fs.h>

#include <asm/uaccess.h>  /* VERIFY_READ|WRITE */

#include "acq32busprot.h"

#include "acq200.h"
#include "acq200_debug.h"
#include "acq200_ioctl.h"

#include "rch.h"

extern int user_max_buf;

/* SHIM */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
#define dma_sync_single_for_cpu		dma_sync_single
#define dma_sync_single_for_device	dma_sync_single
#endif

/* avoid "Thundering Herd" with retry thread when multiple devices */
#define I2O_RETRY(device)	(1 + HZ/10 + (device)->idx)


static void dumpHrd(const char* name, struct HOST_REQUEST_DATA_RECORD* hrd)
{
	dbg(1, "%10s c:%2d %x pci:%08x start:%d n:%d s:%d\n",
	    name, hrd->chan&0x3f, hrd->chan, hrd->pci, hrd->start, 
	    hrd->nsamples, hrd->stride );
}


#define DUMPHRD(var) dumpHrd(#var, var)

#define CHAN(file) ((file)->f_dentry->d_inode->i_rdev&0x00ff)

static Message *build_hrd_message(
	struct file *file, 
	unsigned pa,
	unsigned start,
	unsigned length,
	unsigned stride,
	int outbound,
	unsigned short cflags
	)
{
	Message *message = (Message*)LBUF(file);
	switch(outbound){
	default:
	case 0:
		message->header.type = outbound;
		break;
	case 1:
	case -1:
		message->header.type = 
			MAKE_MT_COMMAND(outbound>0?
					MTC_HOST_REQUEST_DATA_ACQBOUND:
					MTC_HOST_REQUEST_DATA_HOSTBOUND);
	}
	message->header.length = 2*HRDR_SZ;

	message->payload.hrdr[0].pci = pa;
	message->payload.hrdr[0].start = start;
	message->payload.hrdr[0].nsamples = length;
	message->payload.hrdr[0].stride = stride;
	message->payload.hrdr[0].chan = CHAN(file)|cflags;
	
	memset( &message->payload.hrdr[1], 0, HRDR_SZ);

	PD(file)->last_direction_hack = outbound;

	dbg(1, "pci %08x nsamples %d chan %x ob:%d",
		message->payload.hrdr[0].pci,
		message->payload.hrdr[0].nsamples,
		message->payload.hrdr[0].chan,
		outbound);

	return message;
}

static Message *build_outgoing_raw_message(
	struct file *file, 
	unsigned pa,
	unsigned start_byte,
	unsigned nbytes,
	unsigned short cflags
	)
{
	return build_hrd_message(
		file, pa, start_byte/2, nbytes/2, 1, LDH_OUTBOUND, cflags);
}

static Message *build_incoming_raw_message(
	struct file *file, 
	unsigned pa,
	unsigned start_byte,
	unsigned nbytes,
	unsigned short cflags
	)
{
	return build_hrd_message(
		file, pa, start_byte/2, nbytes/2, 1, LDH_INBOUND, cflags);
}


static void buildRch(
	struct ReturnCommandHandler *rch,
	void (*handler)( 
		struct ReturnCommandHandler* this, 
		struct MESSAGE* messsage ),
	struct Acq200Path *path,
	struct IoMapping *kbuf,
	unsigned flags)
{
	rch->handler = handler;
	rch->path = path;
	rch->device = path->device;
	rch->command_id = MAKE_ID_DTACQ(path->device->m_dpd.i2o_last_id_out+1);
	rch->rchReturn.valid = 0;
	rch->rchReturn.nbytes = 0;
	rch->rchReturn.kbuf = kbuf;
	rch->rchReturn.flags = flags;
	rch->lock = &path->device->m_dpd.rch.lock;
}

#define MFA_MASK 0x00ffffff

int acq200_sendI2O_kbuf( struct Acq200Path* path, Message* message )
{
	struct Acq200Device* device = path->device;
	unsigned mfa = readl(CSR(device, ACQ200_IQP));

    
	if (mfa == PCI_I2O_INVALID_MFA){
		dbg(1, "ERROR: no i2o buffers available" );
		return -EACQ32_NO_INCOMING_I2O;
	}else{
		char* pdst = (char*)device->ram.va + (mfa&=MFA_MASK);

		dbg(2,  "write Message %d to mfa 0x%08x\n", 
		    path->device->m_dpd.i2o_last_id_out+1, mfa );
          
		message->header.id =
			MAKE_ID_DTACQ(++device->m_dpd.i2o_last_id_out);

		dbg(3, "message %4d %d 0x%08x 0x%04x 0x%04x\n",
		    message->header.id, 
		    message->header.length,
		    message->header.type,
		    message->payload.sdata[0], 
		    message->payload.sdata[1] );

		dbg(3, "memcpy( %p %p %ld )", pdst, message,
		    (unsigned long)sizeof(MessageHeader)+message->header.length);	

		memcpy(pdst, message, MESSAGE_ACTUAL_LEN(message));

		dma_sync_single_for_device(
			&device->pci_dev->dev, device->ram.pa + mfa, 
			MESSAGE_ACTUAL_LEN(message), 
			DMA_TO_DEVICE);

		dbg(3, "writeln(mfa, CSR( device, ACQ200_IQP) 0x%08x",
		    mfa);
		writel(mfa, CSR(device, ACQ200_IQP));
	
		device->m_dpd.i2o_packets_outgoing++;

		dbg(3, "return 0");
		return 0;
	}
}




int acq200_copy_from_io_to_user(
	struct file *file, void *ubuf, const void *iobuf, int count )
{
	dbg(1, "copy_to_user(%p, %p, %d)", ubuf, iobuf, count);
	if (acq200_stub_copy_to_user){
		return 0;
	}

	return copy_to_user(ubuf, iobuf, count);
}

int acq200_copy_from_user_to_io(
	struct file *file, void *iobuf, const void *ubuf, int count )
{
	dbg(1, "copy_from_user( %p %p %d", iobuf, ubuf, count);
	return copy_from_user(iobuf, ubuf, count);
}


static void i2o_recycleRch(struct ReturnCommandHandler* self)
{

	dbg(2, "self %p self->path %p self->rchReturn.kbuf %p",
	    self, self->path, self->rchReturn.kbuf);

	if (self->path != 0){
		rchRemove(self);
	}
	if (self->rchReturn.kbuf && (self->rchReturn.flags&RCH_KB_POOL)){
		acq200_pool_free_mapping(self->device, self->rchReturn.kbuf);
	}
	acq200_pool_free_rch(self->device, self);
	dbg(2, "99");
}

static void i2o_recycleRchWrapper(
	struct ReturnCommandHandler* self,
	struct MESSAGE *unused)
{
	i2o_recycleRch(self);	
}


static void i2o_handleReadMessageFile(
	struct ReturnCommandHandler* self,
	struct MESSAGE* response
	)
/* output nsamples as self->clidata in BYTES WORKTODO - is this a good idea? */
{
	int nsamples = response->payload.hrdr[0].nsamples;
	struct Acq200Path* path = rchSetSamplesAndGetPath(
			self, response->payload.hrdr[0].start, nsamples);

	DUMPHRD(response->payload.hrdr);
	dbg(2, "nsamples %d path %p %s\n", 
	    nsamples, path, path==0? "RECYCLE": "WAKEUP");

	dbg(1, "rch %p valid %d", self, self->rchReturn.valid);
	if (path != 0){
		wake_up_interruptible(&path->return_waitq);
	}else{
		i2o_recycleRch(self);
	}
}


static void acq200_dev_i2o_return_mfa(
	struct Acq200Device* device, unsigned mfa)
{
	dbg(1,"mfa %08x", mfa);
	writel(mfa, CSR(device, ACQ200_OQP));
}

static void* acq200_dev_i2o_mfa2va(struct Acq200Device *device, u32 mfa)
{
	return (void*)(unsigned long)device->dma_buf.va+(mfa&ACQ200_PCIWINMSK);
}



static int i2o_return_message(struct Acq200Device* device, unsigned mfa)
{
	acq200_dev_i2o_return_mfa(device, mfa);
	device->m_dpd.i2o_packets_returned++;       
	return 0;
}


/*
 * Bottom Halves - ISR task level processing
 */

static void acq200_processCommandMessageReturn(
	struct Acq200Device* device,
	Message* message
	)
// called at bh (ISR-TASK) priority - make it quick
{
	struct ReturnCommandHandler* rch;

	dbg(2,   " locating handler for id %d\n", message->header.id );

	list_for_each_entry(rch, &device->m_dpd.rch.list, list){

		dbg(2, "rch id %d kbuf %p",
		    rch->command_id, rch->rchReturn.kbuf);

		if (rch->command_id == message->header.id){
			if (rch->handler){

				dbg(2, "calling handler for id %d", 
					message->header.id );

				rch->handler( rch, message );
	
			}else{
				dbg(2, "NO HANDLER for %d", 
					message->header.id );
				i2o_recycleRch(rch);
				device->m_dpd.i2o_packets_discarded++;
			}

			return;
		}
	}

/*
 * here if not taken by any handler ...
 */

	dbg(1,   "ERROR - no handler available, discard\n" );
}



void acq200_streaming_isr_bh(unsigned long clidata)
// check the message and distribute to interested parties
// ideally we would have multiple buffers WORKTODO
{
	struct Acq200Device* device = (struct Acq200Device*)clidata;
	u32 mfa;

	TIMESTAMP( device, 'i', "bh" );

	dbg(4, "entry\n" );
    
	while ((mfa = streamGet( &device->streambuf )) != STREAM_MFA_INVALID){
		Message *message = 
			(Message*)acq200_dev_i2o_mfa2va(device, mfa);

		dbg(2,   " mfa 0x%08x type 0x%08x id %04d len %d\n", 
		    mfa, MT_ID(message->header.type), 
		    message->header.id,
		    message->header.length);

		if ( acq200_debug ==7 ){
			dbg(2, "SHORT CIRCUIT\n" );
			i2o_return_message( device, mfa );
			continue;
		}

		switch( MT_ID(message->header.type) ){
#ifdef PGMCOMOUT
		case MT_STREAM:
			if ( device->m_dpd.streaming_enabled ){
				dbg(1,  " MT_STREAM:\n" );
				streamPut( &device->downstreambuf, mfa );
// user code dQ's data
				wake_up_interruptible( &device->m_dpd.waitq ); 
				return;
			}else{
				break;
			}
#endif
		case MT_COMMAND_PACKET:
			acq200_processCommandMessageReturn( device, message );
			break;
		default:
			dbg(1,  " default: id 0x%08x\n", 
			    MT_ID(message->header.type) );

            
			if ( device->m_dpd.aux_task_waiting ){
				device->m_dpd.aux_mfa = mfa;
				device->m_dpd.aux_task_waiting = 0;    
// no race, rather discard
				wake_up_interruptible(&device->m_dpd.aux_waitq);
				return;
			}else{
				device->m_dpd.i2o_packets_discarded++;
				dbg(1,  "discard %x\n", mfa );
			}
		}
		i2o_return_message( device, mfa );              
	}
}




void acq200_consume_i2o(struct Acq200Device* device, u32 mfa )
{
        dbg(4, " OUTBOUND FIFO 0x%08x\n", mfa );

        device->m_dpd.i2o_packets_incoming++;

	if (acq200_debug == 8){
		dbg(2,  "SHORT CIRCUIT\n" );
		i2o_return_message( device, mfa );
	}else if ( streamPut( &device->streambuf, mfa ) < 0 ){
		i2o_return_message( device, mfa );
		device->m_dpd.i2o_packets_discarded++;
		dbg(2, " discarding buffer  0x%08x\n", mfa );

		TIMESTAMP( device, 'i', "disc" );
        }else{
		device->m_dpd.i2o_last_in = (void*)(unsigned long)mfa;

#ifdef PGMCOMOUT
		device->m_dpd.streaming_message_delta_usecs =
			acq32_getDeltaTime( 
				&device->m_dpd.tv_last_streaming_message,
				&device->m_dpd.tv_last_streaming_message  );
#endif
		TIMESTAMP( device, 'i', "put" );

		dbg(4, " queued mfa 0x%08x\n", mfa );

		tasklet_schedule(&device->m_dpd.streaming_isr_tasklet);            
                        
		dbg(5, " queue_task acq200_streaming_isr_bh");
        }
}

static void i2o_handleLastMessage(
	struct ReturnCommandHandler* self,
	struct MESSAGE* response
	)
{
	self->rchReturn.valid = 1;
	wake_up_interruptible(&self->path->return_waitq);
	i2o_recycleRch(self);
}


ssize_t acq200_bridge_read_generic (
	struct file* file, char* buf, size_t count, loff_t* posp,
	int hrd_code,
	struct IoMapping *kbuf)
{
	struct Acq200Device *device = DEV(file);
	struct ReturnCommandHandler* rch = acq200_pool_alloc_rch(device);
	Message *message;
	unsigned pam;
	unsigned rch_flags = RCH_KB_POOL;

	if (rch == 0){
		return -ETIMEDOUT;
	}

	if (kbuf == 0){
		kbuf = acq200_pool_alloc_mapping(
				device, PCI_DMA_FROMDEVICE);	
		if (kbuf == 0){
			i2o_recycleRch(rch);
			return -ETIMEDOUT;
		}
		pam = MK_DMABUF_OFFSET(kbuf, device);
	}else{
		pam = kbuf->pa;
		rch_flags = 0;
	}

	if (kbuf->len == 0){
		i2o_recycleRch(rch);
		acq200_pool_free_mapping(device, kbuf);
		return -ENOMEM;
	}else{
		count = min(count, (size_t)kbuf->len);
	}



	message = build_incoming_raw_message(
			file, pam, *posp, count, hrd_code);

	buildRch(rch, i2o_handleReadMessageFile, PD(file), kbuf, rch_flags);

	dbg(1, "build count %u rch %p kbuf %p", 
	    (unsigned)count, rch,  rch->rchReturn.kbuf);

	rchAppend(&device->m_dpd.rch.list, rch);
	acq200_sendI2O_kbuf(PD(file), message);	

	wait_event_interruptible(
		PD(file)->return_waitq, rch->rchReturn.valid != 0);

	dbg(1, "rch %p valid %d", rch, rch->rchReturn.valid);

	switch(rchRemoveIfNotValid(rch)){
	case 1:
		count = (size_t)rch->rchReturn.nbytes;
		break;
	case 0:
		dbg(1, "return -EINTR rch %p kbuf %p", 
			rch, rch->rchReturn.kbuf);
		/* leave ISR to recycle the buffer when/if it comes */
		return -EINTR;
	default:
		i2o_recycleRch(rch);
		return -EFAULT;
	}

	dbg(1,"copy to user va:%p pa:0x%08lx count:%u",
	    kbuf->va, kbuf->pa, (unsigned)count );

	dma_sync_single_for_cpu(&device->pci_dev->dev,
			rch->rchReturn.kbuf->pa,
			count,
			DMA_FROM_DEVICE);

	acq200_copy_from_io_to_user(file, buf, rch->rchReturn.kbuf->va, count);
	*posp += count;
	i2o_recycleRch(rch);
	
	dbg(1, "return %u", (unsigned)count);
	return count;   
}

ssize_t acq200_bridge_write_generic (
	struct file *file, const char *buf, size_t count, loff_t * posp,
	int hrd_code)
// write data as if from debug port
{	
	struct Acq200Device *device = DEV(file);
	struct ReturnCommandHandler *rch = acq200_pool_alloc_rch(device);
	struct IoMapping *kbuf;
	Message *message;
	unsigned pam;
	unsigned rch_flags = RCH_KB_POOL;
	if (rch == 0) return -ETIMEDOUT;

	kbuf = acq200_pool_alloc_mapping(device, PCI_DMA_TODEVICE);
	if (kbuf == 0){	
		i2o_recycleRch(rch);
		return -ETIMEDOUT;
	}
	pam = MK_DMABUF_OFFSET(kbuf, device);

	count = min(count, (size_t)kbuf->len);

	if (count == 0) return -ENOMEM;


	acq200_copy_from_user_to_io(file, kbuf->va, buf, count);
	
	message = build_outgoing_raw_message( 
		file, pam, *posp, count, hrd_code);

	dma_sync_single_for_device(&device->pci_dev->dev,
			device->dma_buf.pa,
			count,
			DMA_TO_DEVICE);

	buildRch(rch, i2o_recycleRchWrapper, PD(file), kbuf, rch_flags);

	dbg(1,"rch %p kbuf %p mbuf %08x", 
	    rch, rch->rchReturn.kbuf, message->payload.hrdr[0].pci );

	rchAppend(&device->m_dpd.rch.list, rch);
	acq200_sendI2O_kbuf(PD(file), message);

/*
 * and then? - WORKTODO: need to recycle kbuf
 */	
	*posp += count;

	dbg(1, "99 returning %u", (unsigned)count);
	return count;   
}

ssize_t acq200_bridge_write (
	struct file *file, const char *buf, size_t count, loff_t * posp)
// write data as if from debug port
{	
	dbg(1, "count %d", (int)count);

	return acq200_bridge_write_generic (
		file, buf, count, posp,	HRD_CHAN_IS_FILE );
}

ssize_t acq200_bridge_read (
	struct file* file, char* buf, size_t count, loff_t* posp)
{
	return acq200_bridge_read_generic(
		file, buf, count, posp,	HRD_CHAN_IS_FILE, USE_POOL);
}

ssize_t acq200_bridge_readAB (
	struct file* file, char* buf, size_t count, loff_t* posp)
{
	struct Acq200Device *device = DEV(file);
	struct IoMapping mapping;
	/* @todo do the AB toggle thing */
	struct DmaBuf *dma_buf = &device->AB[0];
	
	mapping.va = dma_buf->va;
	mapping.pa = dma_buf->pa;
	mapping.len = min(dma_buf_len(dma_buf), user_max_buf);

	dbg(1, "count %d buffer pa 0x%08lx", (int)count, mapping.pa);

	return acq200_bridge_read_generic(
		file, buf, count, posp,	
			HRD_CHAN_IS_FILE|HRD_ABS_PCI, &mapping);
}

void _recycle(struct Acq200Path *path, int ibuf)
{
	dbg(1, "buffer%c", ibuf? 'B': 'A');

	if (path->rchAB[ibuf]){
		i2o_recycleRch(path->rchAB[ibuf]);	
		path->rchAB[ibuf] = 0;
	}
}

void acq200_bufferABrecycle(struct file *file, struct BufferAB_Ops *ops)
{
	struct Acq200Path *path = PD(file);

	if (ops->ci.recycle&BUFFER_A_FLAG){
		_recycle(path, BUFFERA);
	}
	if (ops->ci.recycle&BUFFER_B_FLAG){
		_recycle(path, BUFFERB);
	}
}

static int fillDeviceStatus(
	struct BufferAB_Ops *ops, struct Acq200Path *path)
{
	int buffer = path->ab_next;
	struct RchReturn* rr = &path->rchAB[buffer]->rchReturn;

	dbg(2, "buffer%c", buffer? 'B': 'A');

	if (rr->valid){		
		dma_sync_single_for_cpu(
			&path->device->pci_dev->dev, rr->kbuf->pa,
			rr->nbytes, DMA_FROM_DEVICE
		);
		ops->ds.full |= BUFFER_FLAG_LUT[buffer];
		ops->ds.sid[path->ab_next] = rr->start_id;
		ops->ds.len[path->ab_next] = rr->nbytes;
		if (rr->nbytes == 0){
			ops->ds.full |= FULL_EOF;
		}
		path->ab_next = buffer == BUFFERA? BUFFERB: BUFFERA;
		return 1;
	}else{
		return 0;
	}
}

static void acq200_bufferAB_queue(struct file *file, int buffer)
{
	struct Acq200Path *path =PD(file);
	struct Acq200Device *device = DEV(file);
	struct ReturnCommandHandler* rch = acq200_pool_alloc_rch(device);
	struct DmaBuf *dma_buf = &device->AB[buffer];
	struct IoMapping mapping;
	Message *message;

	dbg(2, "01 buffer%c", buffer? 'B': 'A');

	path->rchAB[buffer] = rch;

	mapping.va = dma_buf->va;
	mapping.pa = dma_buf->pa;
	mapping.len = min(dma_buf_len(dma_buf), user_max_buf);

	message = build_hrd_message(
		file, mapping.pa, path->sid++, mapping.len/2,
		1, LDH_INBOUND,
		HRD_CHAN_IS_FILE|HRD_ABS_PCI|HRD_READ_NEXT);
	
	buildRch(rch, i2o_handleReadMessageFile, PD(file), 
		 &path->bufferMappings[buffer], !RCH_KB_POOL);
	rchAppend(&device->m_dpd.rch.list, rch);

	dbg(2, "acq200_sendI2O_kbuf %p", message);

	acq200_sendI2O_kbuf(PD(file), message);	
}

int i2o_missed_int_checker(void *arg)
{	
	struct Acq200Device *device = (struct Acq200Device*)arg;
	wait_queue_head_t waitq;

	init_waitqueue_head(&waitq);

	while(!kthread_should_stop()){
		int timeout = wait_event_interruptible_timeout(
			waitq, 0, I2O_RETRY(device));
		if (timeout != 0){
			break;
		}else{
			static int msgn;
			int rc = acq200_handle_incoming_message(device);

			++msgn;
			if (rc == 1){
				info("%d backstop handled lost irq %d", 
				     device->idx, msgn);
			}else{
				if ((msgn&0x1f) == 0){
					dbg(1, "backstop idle %d", msgn);
				}
			}
		}
	}
	return 0;
}

int acq200_bufferABaction(struct file *file, struct BufferAB_Ops *ops)
{
	struct Acq200Path *path = PD(file);
	
	int rc;

	acq200_bufferABrecycle(file, ops);

	if (path->rchAB[BUFFERA] == 0){
		acq200_bufferAB_queue(file, BUFFERA);
	}
	if (path->rchAB[BUFFERB] == 0){
		acq200_bufferAB_queue(file, BUFFERB);
	}

	dbg(2, "wait valid %d timeout %d",
		path->rchAB[path->ab_next]->rchReturn.valid,
		ops->ci.timeout);

	

	rc = wait_event_interruptible_timeout(
		PD(file)->return_waitq, 
		path->rchAB[path->ab_next]->rchReturn.valid != 0,
		ops->ci.timeout);


	dbg(2, "wait done %s", rc==0? "TIMEOUT": rc<0? "ERROR": "OK");
	
	if (rc == 0){
		err("TIMEDOUT");
		return -ETIMEDOUT;
	}else if (rc < 0){
		if (rc != -ERESTARTSYS) err("rc:%d", rc);
		return rc;
	}

	ops->ds.full = 0;
	
	if (fillDeviceStatus(ops, path)){
		fillDeviceStatus(ops, path);
	}
	
	return 0;
}


static void _cleanup_on_bridge_release(struct Acq200Path *path)
{
	struct Acq200Device *device = path->device;
	struct ReturnCommandHandler *rch_cursor;

cleanup:
	list_for_each_entry(rch_cursor, &device->m_dpd.rch.list, list){
		dbg(2, "path %p want %p rch %p kbuf %p",
			    rch_cursor->path, path, rch_cursor, 
			    rch_cursor->rchReturn.kbuf);	 

		if (rch_cursor->path == path){
			dbg(1, "WARNING: last chance to recycle %d",
			    rch_cursor->command_id);
			i2o_recycleRch(rch_cursor);
			goto cleanup;			
		}
	}
}

static int acq200_bridge_release_message(
	struct inode *inode, 
	struct file *file,
	Message *message
)
{
	struct Acq200Path *path = PD(file);
	struct Acq200Device *device = path->device;
	struct ReturnCommandHandler *my_rch = acq200_pool_alloc_rch(device);

	while(my_rch == 0){
		err("null rch. RETRY");
		my_rch = acq200_pool_alloc_rch(device);
	}

	buildRch(my_rch, i2o_handleLastMessage, path, 0, RCH_KB_POOL);
	rchAppend(&device->m_dpd.rch.list, my_rch);	

	acq200_sendI2O_kbuf(path, message);

	wait_event_interruptible(path->return_waitq, 
				 my_rch->rchReturn.valid != 0);

	dbg(1, "done waiting valid: %d", my_rch->rchReturn.valid);

	if (my_rch->rchReturn.valid == 1){
		i2o_recycleRch(my_rch);
	}
	/* this to free any trapped rch */
	_cleanup_on_bridge_release(path);

	return acq200_release(inode, file);
}

int acq200_bridge_release_generic (
	struct inode *inode, struct file *file, int hrd_code
)
{
	struct Acq200Path *path = PD(file);
	
	dbg(2, "path->last_direction_hack %d", path->last_direction_hack);

	switch(path->last_direction_hack){
	case LDH_OUTBOUND:
		return acq200_bridge_release_message(
			inode, file,
			build_outgoing_raw_message(
				file, 0, 0, 0, hrd_code|HRD_CHAN_EOF));
	case LDH_INBOUND:
		return acq200_bridge_release_message(
			inode, file,
			build_incoming_raw_message(
				file, 0, 0, 0, hrd_code|HRD_CHAN_EOF));
	default:
		_cleanup_on_bridge_release(path);
		return acq200_release(inode, file);
	}
}

int acq200_bridge_release (
	struct inode *inode, struct file *file
	)
{
	return acq200_bridge_release_generic(
		inode, file, HRD_CHAN_IS_FILE );
}


void rchInit(struct Acq200Device* device, struct ReturnCommandHandler *rch)
{
	if (rch == 0){
		INIT_LIST_HEAD(&device->m_dpd.rch.list);
		spin_lock_init(&device->m_dpd.rch.lock);
	}else{
		INIT_LIST_HEAD(&rch->list);
		rch->handler = NULL;
		rch->lock = &device->m_dpd.rch.lock;
	}
}

