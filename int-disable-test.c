/* ------------------------------------------------------------------------- */
/* file int-disable-test.c                                                                 */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2011 Peter Milne, D-TACQ Solutions Ltd
 *                      <Peter dot Milne at D hyphen TACQ dot com>
 *  Created on: Jan 6, 2012
 *      Author: pgm

    http://www.d-tacq.com

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

/** @file int-disable-test.c
 *  - runs a "calculation loop".
 *  - first with interrupts on, then masked according to a command line mask
 *
 * Usage: int-disable-test MASK [loopcount]
 */

#include "local.h"
#include <assert.h>

#define IRQ_STATUS "/proc/interrupts"
#define IRQ_MASK   "/dev/acq200/acq200.%d.imask"

int SLOT = 2;
int LOOPCOUNT = 1;
char* MASK = "0";

char* NOMASK = "00000000";



void set_mask(char *mask){
	char knob[80];
	FILE *fp;

	sprintf(knob, IRQ_MASK, SLOT);
	fp = fopen(knob, "w");
	assert(fp);

	fprintf(fp, "%s\n", mask);
	fclose(fp);
}

void test_loop(volatile int loopcount)
{
	volatile unsigned kk = ~0;

	printf("hello loopcount:%d kk:%x\n", loopcount, kk);

	while(loopcount--){

		for(kk = ~0; kk; kk--){
			;
		}
	}
}

void dump_state(char *mask, char* id)
{
	char command[256];
	sprintf(command, "cp %s irqstat.%s.%s", IRQ_STATUS, mask, id);
	system(command);
}
void run_test(char* MASK)
{
	dump_state(MASK, "before");

	test_loop(LOOPCOUNT);

	dump_state(MASK, "after");
}
void init_defaults(int argc, char* argv[])
{
	const char* cp;

	if ((cp = getenv("SLOT")) != 0){
		SLOT = atoi(cp);
		assert(IN_RANGE(SLOT, 2, 10));
	}
	if (argc > 1){
		MASK = argv[1];
		printf("Using MASK: \"%s\"\n", MASK);

		if (argc > 2){
			LOOPCOUNT = atoi(argv[2]);
		}
	}else{
		fprintf(stderr, "USAGE: int_disable_test MASK [LOOPCOUNT]\n");
		exit(1);
	}
}
int main(int argc, char* argv[])
{
	init_defaults(argc, argv);
	run_test(NOMASK);
	set_mask(MASK);
	run_test(MASK);
	set_mask(NOMASK);
}

