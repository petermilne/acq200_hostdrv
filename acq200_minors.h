/* ------------------------------------------------------------------------- */
/* acq200_minors.h - minor device encoding                                   */
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

/** @file acq200_minors.h minor device encoding
 *
 - Scheme copied from ACQ32. But modified to work with ONE Major
 - Minors code as follows:
  - d7:3        0b0xxxxxxx selects channels (0..127)
  - d7:3	0b110xxxxx selects MASTER, SPECIAL
   - 124, 125   : bufA, bufB
   - 128..129   : FILE
   - 227,228,229: RSH
   - 237: ACQCMD
   - 230..255   : SPARE
 *
 */


#define _MASTER(dev)    ((MINOR(dev)		== 0xc0)
#define _SPECIAL(dev)   ((MINOR(dev)&0xe0)	== 0xc0)
#define _CHANNEL(dev)   ((MINOR(dev)&0x80)	== 0x00)
#define _FILE(dev)      ((MINOR(dev)&0xf0)	== 0x80)
#define _RSH(dev) \
	(MINOR(dev)==227 || MINOR(dev)==228 || \
	MINOR(dev)==229 || MINOR(dev)==237)

/*
 * IF _CHANNEL, minor devices code for channel
 *
 * normal channel device returns data as a column for the channel
 * space for 0..63 channels
 */

#define IS_CHANNEL_DEV(dev)    _CHANNEL(dev)
 
#define CHANNEL(dev)    (_CHANNEL(dev)? (MINOR(dev)&0x7f): -1)

/*
 * IF SPECIAL, minor devices code for mastership, ROM or SPECIAL2
 */

#define IS_MASTER(dev)      (_MASTER(dev))

#define SPECIAL_FLAGS(dev)  (_SPECIAL(dev)? MINOR(dev)&0x1f: 0 )

#define IS_FILE(dev)	(_FILE(dev))
#define IS_RSH(dev)	(_RSH(dev))

#define IS_ROM(dev)     (_SPECIAL(dev) && SPECIAL_FLAGS(dev)==0)
#define ROWDEV(dev)     (_SPECIAL(dev) && SPECIAL_FLAGS(dev)==0x1f)
#define DEVRAW(dev) 	(_SPECIAL(dev) && SPECIAL_FLAGS(dev)==1)
#define DEVDMABUF(dev)  (_SPECIAL(dev) && SPECIAL_FLAGS(dev)==2)
#define BIGDUMPDEV(dev) (_SPECIAL(dev) && SPECIAL_FLAGS(dev)==3)
#define AOIDEV(dev)     (_SPECIAL(dev) && SPECIAL_FLAGS(dev)==4)
#define AOFDEV(dev)     (_SPECIAL(dev) && SPECIAL_FLAGS(dev)==5)
#define DOIDEV(dev)     (_SPECIAL(dev) && SPECIAL_FLAGS(dev)==6)
#define DOFDEV(dev)     (_SPECIAL(dev) && SPECIAL_FLAGS(dev)==7)

#define AOFRDEV(dev)    (_SPECIAL(dev) && SPECIAL_FLAGS(dev)==8)
#define DOFRDEV(dev)    (_SPECIAL(dev) && SPECIAL_FLAGS(dev)==9)
#define STATDEV(dev)    (_SPECIAL(dev) && SPECIAL_FLAGS(dev)==10)
#define LLHOSTDEV(dev)	(_SPECIAL(dev) && SPECIAL_FLAGS(dev)==11)


#define TESTDEV(dev)    (_SPECIAL(dev) && SPECIAL_FLAGS(dev)==15)
#define TFTPDEV(dev)    (_SPECIAL(dev) && (SPECIAL_FLAGS(dev)&0x10))

#define HAS_SLAVE( n )  ((SLAVES(dev)&(1<<(n-1)) != 0)
