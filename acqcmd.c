/*****************************************************************************
 *
 * File: acqcmd.c
 *
 * $RCSfile: acqcmd.c,v $
 * 
 * Copyright (C) 1999 D-TACQ Solutions Ltd
 * not to be used without owner's permission
 *
 * Description:
 * send command and get answer from same path - safe in multi task env
 *
 * $Id: acqcmd.c,v 1.13 2007/02/18 14:26:19 pgm Exp $
 * $Log: acqcmd.c,v $
 * Revision 1.13  2007/02/18 14:26:19  pgm
 * *** empty log message ***
 *
 * Revision 1.12  2007/02/18 12:47:53  pgm
 * *** empty log message ***
 *
 * Revision 1.11  2006/08/10 20:03:26  pgm
 * get it right
 *
 * Revision 1.10  2006/08/10 19:52:57  pgm
 * output error on error return
 *
 * Revision 1.9  2006/02/02 21:28:05  pgm
 * trap alarm signal, report and quit
 *
 * Revision 1.8  2006/01/28 17:42:51  pgm
 * *** empty log message ***
 *
 * Revision 1.7  2006/01/13 18:08:03  pgm
 * fix -s bug for x86
 *
 * Revision 1.6  2006/01/12 16:27:42  pgm
 * *** empty log message ***
 *
 * Revision 1.5  2006/01/12 15:51:10  pgm
 * crosscompiles and supports timeout
 *
 * Revision 1.4  2005/08/23 08:56:04  pgm
 * change FNAME default to sensible target value
 *
 * Revision 1.3  2004/12/22 11:31:36  pgm
 * *** empty log message ***
 *
 * Revision 1.2  2004/12/22 11:25:27  pgm
 * *** empty log message ***
 *
 * Revision 1.1.1.1  2004/02/23 07:35:16  pgm
 * ipc orig
 *
 * Revision 1.12  2002/03/18 07:35:33  pgm
 * silent polling
 *
 * Revision 1.11  2002/03/18 07:19:20  pgm
 * poll option
 *
 * Revision 1.10  2002/03/18 06:53:22  pgm
 * *** empty log message ***
 *
 * Revision 1.9  2001/10/07 10:04:08  pgm
 * blanks are coments too
 *
 * Revision 1.8  2001/10/03 06:01:46  pgm
 * reads commands from stdin
 *
 * Revision 1.7  2001/09/07 09:53:44  pgm
 * acqcmd-hold option
 *
 * Revision 1.6  2001/04/07 13:53:58  pgm
 * -b board opt
 *
 * Revision 1.5  2001/03/11 21:30:11  pgm
 * fixed indentation with snavigator
 *
 * Revision 1.4  2001/02/24 18:26:09  pgm
 * acqcmd works
 *
 * Revision 1.3  2001/02/24 18:18:36  pgm
 * test device for lltest, also acqcmd
 *
 *
\*****************************************************************************/

#define acq200_debug verbose
/** @file acqcmd.c <b>CLI application to acq2xx</b>.
  $Revision: 1.13 $

- acqcmd is the general command line interface to acq2xx control. It offers the same interface on
 - CPCI backplane device
 - Remote HUB device
 - run locally on the card itself

- USAGE: acqcmd [opts] [command]
 - if command is omitted, take commands from stdin.
 - -b Board   : send "acqcmd" to Board 
 - -s Board   : run shell command on Board (not used in local command case)

- Examples:
 - Local: acqcmd getNumSamples
 - Backplane: acqcmd -b 2 getNumSamples   (query card in physical slot 2)
 - Hub: acqcmd -b 20 getNumSamples        (query card in logical slot 20)
 - Backplane: acqcmd -s 2 'echo `hostname` `get.release.versions`' (run shell command on remote)
 
 - Specialized Usage
  - --until ST_xxxx   (run until state ST_xxxx eg acqcmd --until ST_STOP)
  - --while ST_xxxx   (run while state ST_xxxx eg acqcmd --while ST_RUN)
  - --timeout T       (timeout on command completion - useful with --until, while).

*/
#include "local.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "popt.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>

#include <errno.h>

#define HELP \
"acqcmd $Revision: 1.13 $ B1005\n"\
"acqcmd [-f device][-b board {1..7}] command\n"\
"or export ACQCMD_DEV=device\n"\
""

#ifdef __arm__
#define FNAME "/tmp/acq200control_command"
#else
#define FNAME "/dev/acq32/acq32.1.m1"
#endif

#define ENV   "ACQCMD_DEV"             /* overrides default */

#define MFMT "/dev/acq32/acq32.%d.m%d"
#define SHELL_FMT "/dev/acq200/acq200.%d.rsh"


poptContext opt_context;
int verbose = 0;


#define MAXLINE 4096

#define PRINTF if ( verbose ) printf

static int process_command( FILE* fd );
static int process_stdin( FILE* fd );
static int do_poll( 
	const char* device,
	FILE* fd, 
	const char* command,
	const char* do_while, 
	const char* do_until );

static int is_socket(const char *file)
{
	struct stat buf;

	stat(file, &buf);
	return (buf.st_mode&S_IFMT) == S_IFSOCK;
}

static int sock;
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>

static int timed_out;

static void alarm_handler(int signum) {
	//printf("alarm");
	timed_out = 1;
	fprintf(stderr, "ERROR:TIMEOUT\n");
	exit( -ETIMEDOUT );
}


static void install_handlers(void) {
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = alarm_handler;
	if (sigaction(SIGALRM, &sa, NULL)) perror ("sigaction");
/*
	struct sigaction sc;
	memset(&sc, 0, sizeof(sc));
	sc.sa_handler = ctrlc_handler;
	if (sigaction(SIGINT, &sc, NULL)) perror ("sigaction");
*/
}


static FILE* make_connection(const char *device)
{
	FILE *fd = 0;

	if (is_socket(device)){
		struct sockaddr_un addr;

		size_t addrLength;

		if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0){
			fprintf( stderr, "ERROR:failed to open socket\n");
			_exit(errno);
		}
		addr.sun_family = AF_UNIX;
		strcpy(addr.sun_path, device);
		addrLength = sizeof(addr.sun_family) + strlen(addr.sun_path);

		if (connect(sock, (struct sockaddr*)&addr, addrLength)){
			fprintf( stderr, "ERROR connect failed \"%s\"\n",
				 device );
		}

		fd = fdopen(sock, "r+");
		if ( fd == NULL ){
			fprintf( stderr, 
				 "ERROR: failed hook socket \"%s\"\n", 
				 device );
			_exit(errno);
		}
	}else{
		fd = fopen( device, "r+" );
		if ( fd == NULL ){
			fprintf( stderr, 
				 "ERROR: failed to open device \"%s\"\n", 
				 device );
			_exit(errno);
		}
	}

	return fd;
}



int main( int argc, const char* argv[] )
{
	char* device = getenv( ENV )? getenv( ENV ):FNAME;
	char devname_buffer[80];
	char* poll_while = 0;
	char* poll_until = 0;
	FILE* fd;
	int timeout = 0;


	int rc;
	int board;
	int poll = 0;

	struct poptOption opt_table[] = {
		{ "device",     'f', POPT_ARG_STRING, &device,     'f' },
		{ "board",      'b', POPT_ARG_INT,    &board,      'b' },
		{ "shell",      's', POPT_ARG_INT,    &board,      's' },
		{ "verbose",    'v', POPT_ARG_INT,    &verbose,    0   },
		{ "version",    0,   POPT_ARG_NONE,   0,           'V' },
		{ "while",      'w', POPT_ARG_STRING, &poll_while, 'p' },
		{ "until",      'u', POPT_ARG_STRING, &poll_until, 'p' },
		{ "timeout",    't', POPT_ARG_INT,    &timeout,    't',
		  "timeout in seconds"},
		POPT_AUTOHELP
		POPT_TABLEEND		
	};

	install_handlers();

	opt_context = poptGetContext( argv[0], argc, argv, opt_table, 0 );

	while ( (rc = poptGetNextOpt( opt_context )) > 0 ){
		switch( rc ){
		case 'V':
			fprintf(stderr, HELP);
			return 0;
		case 'b':
			device = devname_buffer;
			sprintf( device, MFMT, board, board );
			break;
		case 's':
			device = devname_buffer;
			sprintf(device, SHELL_FMT, board);
			break;
		case 'p':
			poll = 1;
			break;
		case 't':
			alarm(timeout);
			break;
		}
	}  // processes all other opts via arg pointers

	if ( verbose > 1 ){
		fprintf( stderr, "using device \"%s\"\n", device );
	}

	fd = make_connection(device);

	if ( STREQ( argv[0], "acqcmd-hold" ) ){
		pause();                             // hold path until killed
		return -1;
	}else if ( poll ) {
		return do_poll( device, fd, poptGetArg( opt_context ), 
				poll_while, poll_until );
	}else if ( poptPeekArg( opt_context ) ){
		PRINTF( "poptPeekArg() returns %p \"%s\"\n",
			poptPeekArg( opt_context ),
			poptPeekArg( opt_context )? 
			poptPeekArg( opt_context ): "---");
		return process_command( fd );
	}else{
		return process_stdin( fd );
	}
}

#define ERROR_STRING	"ACQ32:ERROR"
#define ERROR_LEN	strlen(ERROR_STRING)

static int process_command( FILE* fd )
{       
	char reply[80];
	const char* next_word;
	int nout;
	int err = 0;

	while( (next_word = poptGetArg( opt_context )) ){
		fputs( next_word, fd );
		fputs( " ", fd );
	}

	fputs( "\n", fd );

	for ( nout = 0; fgets( reply, sizeof(reply), fd ) != NULL; ++nout ){
		if (strncmp(reply, ERROR_STRING, ERROR_LEN) == 0){
			err = 1;
		}
		if (STREQ(reply, "EOF\n")){
			break;
		}
		fputs( reply, stdout );
	}

	if ( nout == 0){
		perror( "fgets() ret 0" );
		return -1;
	}else{
		return err;
	}
}

static int do_poll( 
	const char* device,
	FILE* fd, 
	const char* command, 
	const char* do_while, 
	const char* do_until )
{       
	char reply[80];
	int success = -1;

	if( command == 0 ){
		command = "getState";
	}

	PRINTF( "command \"%s\" while \"%s\" until \"%s\"\n",
		command, 
		do_while!=0? do_while: "any",
		do_until!=0? do_until: "none" );


	if (sock){
		fcntl(sock, F_SETFL, O_NONBLOCK);
	}
/*
 * this is a complete and dirty hack 
 */
	while (1) {
		int found_while = 0;
		int found_until = 0;

		PRINTF( "%s\n", command );

		fputs( command, fd );
		fputs( "\n", fd );
		if (sock){
			fflush(fd);
		}

		usleep(100*1000);
		if ( verbose==1 ) fputs( ".", stdout );


		do {
			if (fgets( reply, sizeof(reply), fd ) == 0){
				switch(errno){
				case 0:
					break;
				case EWOULDBLOCK:
					usleep(100*1000);
					break;
				default:
					break;
				}
			}
			if ( verbose > 0 ){
				PRINTF( reply );
			}
			if ( do_while && strstr( reply, do_while ) ){
				found_while = 1;
			}
			if ( do_until && strstr( reply, do_until ) ){
				found_until = 1;
			}
			if (!sock){
				break;
			}
		}while (!strstr(reply, "EOF"));

		if ( verbose==1 ) fputs( "+", stdout );
	    
		if ( found_until ){
			PRINTF( "SUCCESS - found until\n" );
			success = 0;
			break;
		}else if ( do_until==0 && !found_while ){
			PRINTF( "SUCCESS2 - !while\n" );
			success = 0;
			break;
		}else if ((do_while!=0 && found_while) || do_while==0){
			if (!sock){
				/** pci driver caches value */
				PRINTF("reopen(%s)", device);
				fclose(fd);
				fd = make_connection(device);
			}
			continue;
		}else{
			perror( "while or until not found" );
			break;
		}


	}

/*
	if (sock){
		fcntl(sock, F_CLRFL, O_NONBLOCK);	
	}
*/
	return success;
}

#define ISCOMMENT( cl ) (cl[0]=='#')
#define ISBLANK(cl)     (strlen(cl)<=1)

static int iscontinuation(char *line)
{
	int len = strlen(line);

	if (len > 1){
		if (line[len-1] == '\n'){
			switch(line[len-2]){
			case '\\':
			case ';':
				return 1;
			}
		}
	}

	return 0;
}

static int process_stdin( FILE* fd )
{
	char* command_line = malloc(MAXLINE);
	char reply[80];
	int nout;

	while ( fgets( command_line, MAXLINE, stdin ) ){
		PRINTF( command_line );
		if ( !ISCOMMENT( command_line ) && !ISBLANK( command_line ) ){
			fputs( command_line, fd );

			if (iscontinuation(command_line)){
				continue;
			}
			for ( nout = 0; 
			      fgets( reply, sizeof(reply), fd )!=NULL; 
			      ++nout ){
				if (STREQ(reply, "EOF\n")){
					break;
				}
				fputs( reply, stdout );
			}

			if ( nout == 0){
				perror( "fgets() ret 0" );
				return 1;
			}
		}
	}

	free(command_line);
	return 0;
}




