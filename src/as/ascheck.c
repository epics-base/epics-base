/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* share/src/as/ascheck.c */
/* share/src/as $Id$ */
/*	Author: Marty Kraimer	Date: 03-24-94	*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "errlog.h"
#include "asLib.h"
#include "dbStaticLib.h"
int main(int argc,char **argv)
{
    int         i;
    int		strip;
    char	*sub = NULL;
    int		subLength = 0;
    char	**pstr;
    char	*psep;
    int		*len;
    long	status = 0;
    static char *subSep = ",";

    /*Look for options*/
    while(argc>1 && (strncmp(argv[1],"-S",2)==0)) {
	pstr = &sub;
	psep = subSep;
	len = &subLength;
	if(strlen(argv[1])==2) {
	    dbCatString(pstr,len,argv[2],psep);
	    strip = 2;
	} else {
	    dbCatString(pstr,len,argv[1]+2,psep);
	    strip = 1;
	}
	argc -= strip;
	for(i=1; i<argc; i++) argv[i] = argv[i + strip];
    }
    if(argc!=1) {
	printf("usage: ascheck -Smacsub < file\n");
        status = -1;
    } else {
        status = asInitFP(stdin,sub);
        if(status) errMessage(status,"from asInitFP");
    }
    return(status);
}
