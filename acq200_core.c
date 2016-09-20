/* ------------------------------------------------------------------------- */
/* acq200_core.c core driver/hardware interface  functionality               */
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


/** @file acq200_core.c core functionality */


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

#include "acq32busprot.h"




static unsigned _getHeartbeat( struct Acq200Device* device )
{
	return BP_GET_HEARTBEAT(MBOX_READ(device, BP_MB_STATUS));
}

int acq200_getHeartbeat( struct Acq200Device* device )
// return heartbeat
{
	DECLARE_WAIT_QUEUE_HEAD(waitq);
	unsigned v1, v2;
    
	v1 = _getHeartbeat( device );

	init_waitqueue_head( &waitq );    
	dbg(2,  " 1 waitq %p, timeout %d\n", &waitq, HZ/10  );
	interruptible_sleep_on_timeout( &waitq, HZ/10 );
	dbg(2,  " 2\n" );
	v2 = _getHeartbeat( device );
 
	if ( v2 != v1 ){
		if ( v2 > v1 ){
			dbg(2, "acq32_heartbeat %d ticks/sec\n", 
				    (v2-v1)*10 );
			return device->heartbeat_detected = v2 - v1;
		}else{
			dbg(2,  " 3\n" );
			init_waitqueue_head( &waitq );    
			interruptible_sleep_on_timeout( &waitq, HZ/10 );
			dbg(2,  " 4\n" );
			v1 = _getHeartbeat( device );
	    
			dbg(2, " second try %d ticks/sec\n",
				    (v1-v2)*10 );

			return device->heartbeat_detected = v1 - v2;
		}
	}else{
		unsigned m1, m2, m3, m4;
            
		m1 = MBOX_READ(device, 0);
		m2 = MBOX_READ(device, 1);
		m3 = MBOX_READ(device, 2);
		m4 = MBOX_READ(device, 3);
		dbg(1, "acq32_heartbeat ERROR:mailboxes are\n"
			    "0x%08x\n0x%08x\n0x%08x\n0x%08x\n",
			    m1, m2, m3, m4 );
		return device->heartbeat_detected = 0;
	}
}


int acq200_getAvailableChannels(struct Acq200Device* device)
{
	int rc = acq200_sendQuery(
		device,
		BP_SET_FUNCODE(BPFC_GET_NCHANNELS),
		&device->input_channels.nchannels, 
		&device->output_channels.nchannels,
		NULL_A3,
		NULL_A4
		);

	if (rc != 0){
		err("failed %d", rc);
	}
	return rc;
}
