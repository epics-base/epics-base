/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *	Author:	Jeff Hill
 *	Date:	21JAN2000
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
		printf("usage: %s <PV name> [optional value to be written]\n", argv[0]);
		printf("the following arguments were received\n");
		while(argc>0) {
			printf("%s\n",argv[0]);
			argv++; argc--;
		}
		return -1;
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
		return -1;
	}

}
