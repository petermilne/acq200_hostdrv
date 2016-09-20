/* ------------------------------------------------------------------------- */
/* htstream.c High Throughput Streaming for ACQ196                           */
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
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                   */
/* ------------------------------------------------------------------------- */


/** @file htstream.c <b>High Throughput Streaming for ACQ196</b>.
 * $Revision: 1.11 $
 * - File <a href="../HTSTREAM.README">HTSTREAM.README</a> main htstream readme
 * - File <a href="../setup.htstream.multi">setup.htstream.multi</a> initialisation
 * for one or more ACQ196 cards
 * - File <a href="../setup.htstream.multi">setup.htstream.acq164</a> initialisation
 * for one or more ACQ164 cards
 *
 *
 * - "User Interface" is via environment variables.
 *
 */
#define ACQ200_DEBUG 		"ACQ200_DEBUG"
/**< set debug level [0]. */
#define ACQ200_CHECKWORK 	"ACQ200_CHECKWORK"
/**< force htstream to do work (compute md5sum of data). */
#define ACQ200_SLEEP_WORK	"ACQ200_SLEEP_WORK"
/**< simulate work by sleeping (usecs). */
#define ACQ200_TIMEOUT "ACQ200_TIMEOUT"
/**< timeout in TICKS before reporting timeout error. */
#define ACQ200_SLOT "ACQ200_SLOT"
/**< target card SLOT number. */
#define ACQ200_MAXTO "ACQ200_MAXTO"
/**< maximum timeouts to accept before dropping out. */
#define ACQ200_WRITEFILE "ACQ200_WRITEFILE"
/**< file pattern to output to [default: no output]. */
#define ACQ200_MAXFILE "ACQ200_MAXFILE"
/**< maximum file length [default: no limit]. */
#define ACQ200_NFILES "ACQ200_NFILES"
/**< number of files to write before repeating. */
#define ACQ200_ONESHOT "ACQ200_ONESHOT"
/**< stop after NFILES. */

#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

#include <math.h>

#include "local.h"
#include "acq200_ioctl.h"



#define BUFLEN	0x400000	/* @@todo read from device */
#define TIMEOUT	50		/* @@todo ticks. should be msec. */

				/*         but that needs dvr chg. */

#define INITIAL_TIMEOUT (24*3600*100)

#define REVID "$Revision: 1.11 $ B1000"


int SLOT = 3;
int acq200_debug = 0;
int timeout = TIMEOUT;
int MAXTO = 60;			/* max timeout retries */
int SLEEP_WORK = 0;

struct DeviceMapping {
	int fd;
	void *pbuf;
	char id[8];
};

struct FILE_INFO {
	char format[128];
	char fname[128];
	unsigned maxfile;
	unsigned nfiles;
	int ifile;
	int fd;
	int oneshot;
}
	G_file = {
		/* default limits: 2GB * 10000 = 10T */
		.maxfile = 0x80000000U,
		.nfiles = 10000,
		.fd = -1
	};

static struct DeviceMapping bufAB[2] = {
	[BUFFERA] = {
		.id = "bufA"
	},
	[BUFFERB] = {
		.id = "bufB"
	}
};


static void initMapping(struct DeviceMapping *mapping, int slot)
{
	char fname[128];

	sprintf(fname, "/dev/acq200/acq200.%d.%s", slot, mapping->id);

	mapping->fd = open(fname, O_RDWR);
        if (mapping->fd < 0){
		err("failed to open %s ", fname);
		perror("open");
		exit(errno);
	}

	mapping->pbuf = mmap(0, BUFLEN, 
				PROT_READ|PROT_WRITE, 
				MAP_PRIVATE|MAP_NONBLOCK, 
				mapping->fd, 0);

	if (mapping->pbuf == MAP_FAILED){
		err("mmap() failed");
		perror("mmap");
		exit(errno);
	}
}

static void checkWork(int index, int len)
{
	FILE *fp = popen("md5sum", "w");

	dbg(1, "index %d len %d", index, len);
	fwrite(bufAB[index].pbuf, 1, len, fp);

	pclose(fp);
}



static void fileOpen(int len)
{
	char fname[128];
	if (G_file.fd != -1 ){
		off_t fp = lseek(G_file.fd, 0, SEEK_CUR);
		if (fp + len < G_file.maxfile){
			return;
		}else{
			close(G_file.fd);
		}
	}

	sprintf(G_file.fname, G_file.format, G_file.ifile);
	G_file.fd = open(G_file.fname, 
				O_WRONLY|O_CREAT|O_TRUNC|O_LARGEFILE, S_IRWXU);
	
	if (G_file.fd <= 0){
		perror("failed to open file");
		err("FATAL open failed:%s", fname);
		exit(-1);
	}

	if (++G_file.ifile > G_file.nfiles){
		if (G_file.oneshot){
			exit(0);
		}
		G_file.ifile = 0;
	}
}

static void fileWork(int index, int len)
{
	dbg(2, "%s: index %d len %d", G_file.fname, index, len);
	fileOpen(len);
	write(G_file.fd, bufAB[index].pbuf, len);
}

static void defaultWork(int index, int len)
{
	
}


static void sleepWork(int index, int len)
{
	dbg(1, "%d", SLEEP_WORK);
	usleep(SLEEP_WORK);
}

void (* doWork)(int index, int len) = defaultWork;

static int action() {
	struct BufferAB_Ops ops = {
		.ci.timeout = INITIAL_TIMEOUT
	};
	int next = BUFFERA;
	char status[80];
	unsigned long long total = 0;
	int syscalls = 1;
	int timeouts = 0;
#define FD bufAB[BUFFERA].fd
	int started = 0;


	dbg(1, "timeout %d", ops.ci.timeout);

	initMapping(bufAB+0, SLOT);
	initMapping(bufAB+1, SLOT);


	for(; 1; syscalls++, ops.ci.timeout = timeout){
		int rc = ioctl(FD, ACQ200_BUFFER_AB_IOWR, &ops);
		char *sp = status;
#define SPRINTF(fmt, arg...) do { sp += sprintf(sp, fmt, ##arg); } while(0)

		if (rc >= 0){
			if (acq200_debug && timeouts != 0){
				SPRINTF("\n");				
			}
			timeouts = 0;
			started = 1;
		}else if (rc == -1 && errno == ETIMEDOUT){
			if (acq200_debug){
				fputc('.', stderr); fflush(stderr);
			}
			if (started && ++timeouts >= MAXTO){
				perror("TIMEOUT?");
				return rc;
			}else{
				sleep(1);
				continue;
			}
		}else{
			perror("errno?");
			err("ioctl(ACQ200_BUFFER_AB_IOWR) %d errno %d ET %d",
						rc, errno, ETIMEDOUT);
//			return rc;
			continue;
		}
		
		ops.ci.recycle = 0;

		SPRINTF("%8d:", syscalls);
		if (ops.ds.full&BUFFER_FLAG_LUT[next]){
			total += ops.ds.len[next];
			SPRINTF("N:%d %8d %8d :", next, 
					ops.ds.sid[next], ops.ds.len[next]);
			ops.ci.recycle |= BUFFER_FLAG_LUT[next];

			doWork(next, ops.ds.len[next]);

			next = !next;

			if (ops.ds.full&BUFFER_FLAG_LUT[next]){
				total += ops.ds.len[next];
				SPRINTF("..");
				SPRINTF("N:%d %8d %8d :", next, 
					ops.ds.sid[next], ops.ds.len[next]);
				ops.ci.recycle |= BUFFER_FLAG_LUT[next];
				doWork(next, ops.ds.len[next]);


				next = !next;
			}
		}
		dbg(1, "%s", status);

		if (ops.ds.full&FULL_EOF){
			break;
		}
	}

	fprintf(stderr, "Total bytes: %Lu\n", total);

	return 0;
}


int main(int argc, char* argv[])
{	
	if (getenv(ACQ200_DEBUG)){
		acq200_debug = atoi(getenv(ACQ200_DEBUG));
	}
	if (getenv(ACQ200_CHECKWORK)){
		doWork = checkWork;
	}
	if (getenv(ACQ200_SLEEP_WORK)){
		SLEEP_WORK= MAX(atoi(getenv(ACQ200_SLEEP_WORK)), 1);
		doWork = sleepWork;
	}
	if (getenv(ACQ200_TIMEOUT)){
		timeout = atoi(getenv(ACQ200_TIMEOUT));
	}
	if (getenv(ACQ200_SLOT)){
		SLOT = atoi(getenv(ACQ200_SLOT));
	}
	if (getenv(ACQ200_MAXTO)){
		MAXTO = atoi(getenv(ACQ200_MAXTO));
	}
	if (getenv(ACQ200_WRITEFILE)){
		strcpy(G_file.format, getenv(ACQ200_WRITEFILE));
		if (strstr(G_file.format, "%") == 0){
			strcat(G_file.format, ".%03d");
		}
		if (getenv(ACQ200_MAXFILE)){
			G_file.maxfile = strtoul(getenv(ACQ200_MAXFILE),0,0);
		}		
		if (getenv(ACQ200_NFILES)){
			G_file.nfiles = atoi(getenv(ACQ200_NFILES));
		}
		if (getenv(ACQ200_ONESHOT)){
			G_file.oneshot = atoi(getenv(ACQ200_ONESHOT));
		}
		doWork = fileWork;
	}

	info("htstream %s slot %d", REVID, SLOT);

	return action();
}
