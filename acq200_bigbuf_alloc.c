/* ------------------------------------------------------------------------- */
/* acq200_bigbuf_alloc.c allocator to share bigbuf                           */
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

/** @file acq200_bigbuf_alloc.c buffer pool */

#include <linux/fs.h>     /* everything... */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>        /* O_ACCMODE */
#include <linux/pci.h>
#include <linux/time.h>

#include <linux/interrupt.h>

#include <asm/io.h>       /* ioremap()         */
#include <asm/uaccess.h>  /* VERIFY_READ|WRITE */
#include <asm/system.h>   /* cli(), *_flags */
#include <asm/segment.h>  /* memcpy and such */

#include "acq200.h"
#include "acq200_debug.h"
#include "rch.h"


static void create_dev_sysfs(struct device *dev);
static void remove_dev_sysfs(struct device *dev);

#define NDATAMAPPING (NMAPPINGS-1)

#define MSG_MAPPING (NMAPPINGS-1)

#define NHANDLERS 16

#define HANDLERS_SIZE (sizeof(struct ReturnCommandHandler)*NHANDLERS)

struct PoolEntry {
	int reserved;
	void* item;
};

struct BigBufPool {
	struct Pool {
		int max_entries;
		struct PoolEntry *entries;
		int cursor;
		char name[16];
	}
		mappings, handlers;

	wait_queue_head_t sleepq;
};

static void _create_mappings(
	struct Acq200Device *device,
	struct BigBufPool *pool 
)
{
	struct DmaBuf *bb = &device->dma_buf;
	struct PoolEntry *entries = (struct PoolEntry*)
		KMALLOC(NMAPPINGS*sizeof(struct PoolEntry), GFP_KERNEL);
	struct IoMapping *mappings = (struct IoMapping*)
		KMALLOC(sizeof(struct IoMapping)*NMAPPINGS, GFP_KERNEL);

	int ii;
	int buflen = dma_buf_len(&device->dma_buf)/NMAPPINGS;
	unsigned offset = 0;

	assert(mappings);
	assert(entries);

	pool->mappings.entries = entries;
	pool->mappings.max_entries = NMAPPINGS;
	pool->mappings.cursor = 0;
	sprintf(pool->mappings.name, "mappings");

	for (ii = 0; ii != NMAPPINGS; ++ii, offset += buflen){
		struct IoMapping *mapping = &mappings[ii];

		mapping->va = (u32*)((char*)bb->va + offset);
		mapping->pa = bb->pa + offset;
		mapping->len = buflen;
		sprintf(mapping->name, "buf %d.%d", device->idx, ii );

		entries[ii].reserved = 0;
		entries[ii].item = mapping;
	}
}

static void _delete_mappings(struct Acq200Device *device)
{
	KFREE(device->pool->mappings.entries[0].item);
	KFREE(device->pool->mappings.entries);
}

static void _create_handlers(
	struct Acq200Device *device,
	struct BigBufPool *pool
	)
{
	struct PoolEntry *entries = (struct PoolEntry*)
		KMALLOC(NHANDLERS*sizeof(struct PoolEntry), GFP_KERNEL);
	struct ReturnCommandHandler* handlers = (struct ReturnCommandHandler*)
		KMALLOC(HANDLERS_SIZE, GFP_KERNEL);
	int ii;

	assert(entries);
	assert(handlers);

	memset(handlers, 0, HANDLERS_SIZE);

	pool->handlers.entries = entries;
	pool->handlers.max_entries = NHANDLERS;
	pool->handlers.cursor = 0;
	sprintf(pool->handlers.name, "handlers");
	
	for (ii = 0; ii != NHANDLERS; ++ii){
		rchInit(device, &handlers[ii]);
		entries[ii].reserved = 0;
		entries[ii].item = handlers + ii;
	}
}

static void _delete_handlers(struct Acq200Device *device)
{
	KFREE(device->pool->handlers.entries[0].item);
	KFREE(device->pool->handlers.entries);
	
}

void acq200_pool_create(struct Acq200Device *device)
{
	struct BigBufPool *pool = (struct BigBufPool*)
		KMALLOC(sizeof(struct BigBufPool), GFP_KERNEL);

	memset(pool, 0, sizeof(struct BigBufPool));
	_create_mappings(device, pool);
	_create_handlers(device, pool);

	pool->mappings.entries[MSG_MAPPING].reserved = 1;
	init_waitqueue_head(&pool->sleepq);
	device->pool = pool;

	create_dev_sysfs(&device->pci_dev->dev);       
}

void acq200_pool_delete(struct Acq200Device *device)
{
	if (device->pool){
		remove_dev_sysfs(&device->pci_dev->dev);
		_delete_mappings(device);
		_delete_handlers(device);
		KFREE(device->pool);
		device->pool = 0;
	}
}





static void *_alloc(
	struct BigBufPool *big_pool,
	struct Pool *this_pool)
{
#define INCR(ibuf) (ibuf >= this_pool->max_entries-1? 0: ibuf+1)	
	int ibuf;
	struct PoolEntry *pe = this_pool->entries;

	for (ibuf = INCR(this_pool->cursor); pe[ibuf].reserved; 
		ibuf = INCR(ibuf)){
		if (ibuf == this_pool->cursor){
			if ( interruptible_sleep_on_timeout(
				     &big_pool->sleepq, 1000) == 0 ){
				info( " TIMEOUT" );
				return 0;
			}
		}
	}
	
	this_pool->cursor = ibuf;
	pe[ibuf].reserved = 1;
	return pe[ibuf].item;
}


static void _free(
	struct BigBufPool *big_pool, 
	struct Pool *this_pool, 
	void *item)
{
	int ibuf;
	struct PoolEntry *pe = this_pool->entries;

	for (ibuf = 0; ibuf != this_pool->max_entries; ++ibuf){
		if (pe[ibuf].item == item){
			pe[ibuf].reserved = 0;
			wake_up_interruptible(&big_pool->sleepq);
			return;
		}
	}

	err("FAILED to free item %p", item);
}



struct IoMapping *acq200_pool_alloc_mapping(
	struct Acq200Device *device, int direction
	)
{
	struct IoMapping *map = (struct IoMapping*)_alloc(
		device->pool, &device->pool->mappings);

	dbg(2, "map %p", map);
	return map;
}

void acq200_pool_free_mapping(
	struct Acq200Device *device, struct IoMapping *mapping)
{
	dbg(2, "free %p", mapping);
	_free(device->pool, &device->pool->mappings, mapping);
}

struct ReturnCommandHandler *acq200_pool_alloc_rch(
				struct Acq200Device *device)
{
	struct ReturnCommandHandler *rch =(struct ReturnCommandHandler*)
		_alloc(device->pool, &device->pool->handlers);

	dbg(2, "alloc %p", rch);
	return rch;
}

void acq200_pool_free_rch(
	struct Acq200Device *device, struct ReturnCommandHandler *h)
{
	dbg(2, "free %p", h);	       
	_free(device->pool, &device->pool->handlers, h);
}
	


static ssize_t show_pool(
	struct device *dev, 
	struct device_attribute *attr,
	char * buf)
{
	struct Acq200Device *device = acq200_lookupDevice(dev);
	int len = 0;
	int ii;
	struct BigBufPool *bb_pool = device->pool;
	struct PoolEntry* pe;
#define FFMT "%20s: "
#define STAPRINT(fmt, field) \
        len += sprintf(buf+len,  FFMT fmt "\n", #field, field)

        if (device == 0){
		return sprintf(buf, "ERROR: device not found\n");
	}

		
	len += sprintf(buf+len, "MAPPINGS\n");
	pe = bb_pool->mappings.entries;
	for (ii = 0; ii != bb_pool->mappings.max_entries; ++ii){
		len += sprintf(buf+len, "[%2d] %c %p %d\n",
			       ii, ii==bb_pool->mappings.cursor? '*':' ',
				pe[ii].item, pe[ii].reserved);
	}

	len += sprintf(buf+len, "HANDLERS\n");
	pe = device->pool->handlers.entries;
	for (ii = 0; ii != bb_pool->handlers.max_entries; ++ii){
		struct ReturnCommandHandler *rch = 
			(struct ReturnCommandHandler *)pe[ii].item;

		len += sprintf(buf+len, 
				"[%2d] %c %p %d hdlr:%p kbuf %p valid %d\n",
			       ii, 
				ii==bb_pool->handlers.cursor? '*':' ',
				pe[ii].item, pe[ii].reserved,
			       rch->handler, rch->rchReturn.kbuf,
				rch->rchReturn.valid	);
	}
	return len;
#undef STAPRINT
}

static DEVICE_ATTR(pool, S_IRUGO, show_pool, 0);


static ssize_t show_rch_list(	struct device *dev, 
	struct device_attribute *attr,
	char * buf)
{
	struct Acq200Device *device = acq200_lookupDevice(dev);
	int len = 0;
	struct ReturnCommandHandler *rch;

        if (device == 0){
		return sprintf(buf, "ERROR: device not found\n");
	}

		
	len += sprintf(buf+len, "RCH LISTING\n");

	list_for_each_entry(rch, &device->m_dpd.rch.list, list){
		len += sprintf(buf+len, 
				"%p hdlr:%p kbuf %p valid %d path %p\n",
			       rch, rch->handler, rch->rchReturn.kbuf,
			       rch->rchReturn.valid,
			       rch->path  );		
	}
	return len;
#undef STAPRINT
}

static DEVICE_ATTR(rch_list, S_IRUGO, show_rch_list, 0);

static void create_dev_sysfs(struct device *dev)
{
	DEVICE_CREATE_FILE(dev, &dev_attr_pool);
	DEVICE_CREATE_FILE(dev, &dev_attr_rch_list);
}

static void remove_dev_sysfs(struct device *dev)
{
	device_remove_file(dev, &dev_attr_pool);
	device_remove_file(dev, &dev_attr_rch_list);
}
