/*	@(#)ca_test_main.c	$Id$
 *	Author:	Jeff Hill
 *	Date:	21JAN2000
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01	21JAN2000	mrk	split main from ca_test
 */
#include <string.h>
#include <stdio.h>

#include "ca_test.h"
#include "dbDefs.h"

int main(int argc, char **argv)
{

	/*
	 * print error and return if arguments are invalid
	 */
	if(argc < 2  || argc > 3){
		printf("usage: ca_test channel_name <\"put value\">\n");
		printf("the following arguments were received\n");
		while(argc>0) {
			printf("%s\n",argv[0]);
			argv++; argc--;
		}
		return ERROR;
	}


	/*
	 * check for supplied value
	 */
	if(argc == 2){
		return ca_test(argv[1], NULL);
	}
	else if(argc == 3){
		char *pt;

		/* strip leading and trailing quotes*/
		if(argv[2][1]=='"') argv[2]++;
		if( (pt=strchr(argv[2],'"')) ) *pt = 0;
		return ca_test(argv[1], argv[2]);
	}
	else{
		return ERROR;
	}

}
