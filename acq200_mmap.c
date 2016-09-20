/* ------------------------------------------------------------------------- */
/* acq200_mmap.c driver memory mapping functions for acq200 peripheral       */
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

#include <asm/pgtable.h>  /* io_remap_pfn_range */
#include <asm/uaccess.h>  /* VERIFY_READ|WRITE */

#include "acq200_debug.h"
#include "acq200.h"
#include "acq200_minors.h"



#define PCI_BA_CSR  0 /* surely there's an official def ?? WORKTODO */
#define CSR_SIZE    0x80
#define CSR_SIZE_MAX 0x1000

static int acq200_makeIoMapping(
	struct Acq200Device* device, int bar, const char* ident, int len)
// return 0 on success
{
#define BA_MASK PCI_BASE_ADDRESS_MEM_MASK
	struct IoMapping* pim = &device->csr;
	int rc = 0;

	snprintf(pim->name, sizeof(pim->name), "%s.%s", 
		 device->ldev.drv_name, ident);

	pim->pa = pci_resource_start(device->pci_dev, bar)&BA_MASK;
	pim->len = len;
    
	dbg(1, "request_mem_region 0x%08lx %d %s\n", 
		pim->pa, pim->len, pim->name );

	if ( request_mem_region( pim->pa, pim->len, pim->name ) != 0 ){
		pim->va = ioremap_nocache( pim->pa, pim->len );
		dbg(1, "ioremap_nocache pa: 0x%08lx len: 0x%02x va: 0x%p", 
		    pim->pa, pim->len, pim->va);
	}else{
		dbg( 1, "request_mem_region( 0x%08lx, %d, %s ) failed\n",
			    pim->pa, pim->len, pim->name );
		rc = -ENODEV;
	}

	return rc;
}


int acq200_map_pci_memory(struct Acq200Device* device)
/**
 *  map remote regs area on device load
 */
{
    int rc = 0;                /* assume success until fail */
    
    if ( (rc = acq200_makeIoMapping( device, PCI_BA_CSR, "csr", CSR_SIZE)) != 0 ){
        return rc;
    }
    return rc;
}

static void clearIoMapping(struct IoMapping* iomap)
{
    if (iomap->va){
        iounmap(iomap->va);
    }
    if (strlen( iomap->name ) ){
        release_mem_region(iomap->pa, iomap->len);
    }
    memset( iomap, 0, sizeof(struct IoMapping));     
}

void acq200_unmap_pci_memory( struct Acq200Device* device )
/**
 * unmap remote regs are on device unload
 */
{
    clearIoMapping(&device->csr);
}


static inline int getDirectionFromVma(struct vm_area_struct *vma)
{
	int direction;
/* 
 * VM_READ: client wants to READ the MAPPING, DMA fills it FROM_DEVICE
 * VM_WRITE, client wants to WRITE the MAPPING, DMA write TO_DEVICE
 */
	switch(vma->vm_flags&(VM_READ|VM_WRITE)){
	case VM_READ|VM_WRITE:
		direction = DMA_BIDIRECTIONAL;
		break;
	case VM_READ:
		direction = DMA_FROM_DEVICE;
		break;
	case VM_WRITE:
		direction = DMA_TO_DEVICE;
		break;
	default:
		direction = DMA_NONE;
		break;		
	}
	return direction;
}


static int mmap_dmabuf(
		struct file* file, struct vm_area_struct* vma,
		struct DmaBuf *dma_buf, char id)
{
	unsigned long vsize = vma->vm_end - vma->vm_start;
	unsigned long psize = dma_buf_len(dma_buf);
	unsigned pfn = dma_buf->pa >> PAGE_SHIFT;

	dbg(1, "%c vsize %lu psize %lu %s",
		id, vsize, psize, vsize>psize? "EINVAL": "OK");

	if (vsize > psize){
		return -EINVAL;                   /* request too big */
	}
	if (remap_pfn_range(
		vma, vma->vm_start, pfn, vsize, vma->vm_page_prot)){
		STATUS;
		return -EAGAIN;
	}else{
		STATUS;
		return 0;
	}
}
int acq200_mmap_host(struct file* file, struct vm_area_struct* vma)
/**
 * mmap the host buffer.
 */
{
	return mmap_dmabuf(file, vma, &PD(vma->vm_file)->dma_buf, 'D');
}

int acq200_mmap_llhost(struct file* file, struct vm_area_struct* vma)
/**
 * mmap the host buffer.
 */
{
	return mmap_dmabuf(file, vma, &PD(vma->vm_file)->llhost_dma_buf, 'L');
}

int acq200_mmapAB(struct file* file, struct vm_area_struct* vma)
/**
 *  provide user mapping to mailboxes on target. follows LDD3.pg429
 */
{
	struct Acq200Device* device = DEV(file);
	int ibuf = PD(file)->minor&1;
	struct DmaBuf *dma_buf = &device->AB[ibuf];

	return mmap_dmabuf(file, vma, dma_buf, ibuf? 'B': 'A');
}

int acq200_mmap_mbox(struct file* file, struct vm_area_struct* vma)
/**
 *  provide user mapping to mailboxes on target. follows LDD3.pg429
 */
{
	struct Acq200Device* device = DEV(file);
	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long pa  = device->csr.pa + off;
	unsigned pfn = pa >> PAGE_SHIFT;
	unsigned long vsize = vma->vm_end - vma->vm_start;
	unsigned long psize = CSR_SIZE_MAX;

	STATUS;

	if (vsize > psize){
		return -EINVAL;                   /* request too big */
	}
	if (remap_pfn_range(
		vma, vma->vm_start, pfn, vsize, vma->vm_page_prot)){
		STATUS;
		return -EAGAIN;
	}else{
		STATUS;
		return 0;
	}
}
