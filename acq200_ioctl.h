/* ------------------------------------------------------------------------- */
/* acq200_ioctl.h defines ioctls and interface parameters for acq200 hostdrv */
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


#ifndef __ACQ200_IOCTL_H__
#define __ACQ200_IOCTL_H__

#include <asm/ioctl.h>
#include <asm/types.h>

#define DMAGIC 0xDA

#define MBOX_NUM 4
#define MBC_FLAGS_TO		0xffff    /* timeout in jiffies */
#define MBC_FLAGS_TO_FOREVER	0
#define MBC_FLAGS_MBXW		0x000f0000 /* mask mailboxes to write */
#define MBC_FLAGS_MBXR          0x00f00000 /* mask mailboxes to read  */
#define MBC_FLAGS_ONRELEASE	0x10000000 /* action this command on release */

#define MBC_TO_MBXW(mx)		((mx) << 16)
#define MBC_FROM_MBXW(mbc)	(((mbc->mbc_flags)&MBC_FLAGS_MBXW) >> 16)

#define MBC_TO_MBXR(mx)		((mx) << 20)
#define MBC_FROM_MBXR(mbc)	(((mbc->mbc_flags)&MBC_FLAGS_MBXR) >> 20)

struct MailboxControl {
	u32 mbc_flags;
	u32 mbxw[MBOX_NUM];     /* mailbox write values */
	u32 mbxr[MBOX_NUM];     /* mailbox read values  */	
};


/** immediate read */
#define ACQ200_MBOX_IOR		_IOR(DMAGIC,   1, struct MailboxControl)
/** non-blocking write ... NB dangerous to mix with blocking write */
#define ACQ200_MBOX_IOW         _IOW(DMAGIC,   2, struct MailboxControl)
/** write block read return */
#define ACQ200_MBOX_IOWR	_IOWR(DMAGIC,  3, struct MailboxControl)



enum {
    EACQ32_NO_INCOMING_I2O    = 3200,
    EACQ32_NO_MAPPING,
    EACQ32_OUT_OF_MAPPING_RANGE,
    EACQ32_OFFSET_NOT_ON_WHOLE_BUFFER_BOUNDARY
};

/** Swinging buffer operation
 *
 * BUFFER_A, BUFFER_B : filled in turn.
 * need to know - sequence number, length
 * need to return buffer to the pool when doen 
 * a single ioctl call will:
 * - recycle buffers
 * - block on timeout
 * - return status of both BufferA, BufferB
 */

#define BUFFERA	0
#define BUFFERB 1

#define BUFFER_A_FLAG	0x1
#define BUFFER_B_FLAG	0x2

#define BUFFER_FLAG_LUT "\x1\x2"

#define FOREVER	((long)(~0UL>>1))	/* MAX_SCHEDULE_TIMEOUT */

#define FULL_EOF	0x80000000

struct BufferAB_Ops {
	struct ClientInstructions {
		unsigned recycle;      /* client returns buff to circulation */
		unsigned timeout;      /* timeout ticks to block for */
	}
		ci;
	struct DriverStatus {
		unsigned full;		/* buffer full flags */
		unsigned sid[2];	/* buffer sequence id numbers */
		unsigned len[2];	/* data length in buffer (bytes) */
	}	 
		ds;
};
#define BUFFERAB_OPS_SZ	(sizeof(struct BufferAB_Ops))

#define ACQ200_BUFFER_AB_IOWR	_IOWR(DMAGIC, 4, struct BufferAB_Ops)


#endif /* __ACQ200_IOCTL_H__ */
