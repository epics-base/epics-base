/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*	Author: Marty Kraimer	Date: 03-24-94	*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "asLib.h"
#include "dbStaticLib.h"
#include "errlog.h"

int main(int argc,char **argv)
{
    int         argn = 1;
    char	*sub = NULL;
    int		subLength = 0;
    char	**pstr;
    char	*psep;
    int		*len;
    long	status = 0;
    static char *subSep = ",";

    /* Look for -Smacro=value options */
    while (argc>argn && (strncmp(argv[argn], "-S", 2)==0)) {
	pstr = &sub;
	psep = subSep;
	len = &subLength;
	if (strlen(argv[argn])==2) {
	    dbCatString(pstr, len, argv[++argn], psep);
	} else {
	    dbCatString(pstr, len, argv[argn]+2, psep);
	}
	argn++;
    }
    if (argc == argn) {
	status = asInitFP(stdin, sub);
	if(status) errlogPrintf("ascheck: Access Security File failed.\n");
    } else if (argc == argn+1) {
	status = asInitFile(argv[argn], sub);
	if(status) errlogPrintf("ascheck: Access Security File failed.\n");
    } else {
	printf("usage: ascheck [-Smac=sub ...] [<] file\n");
	status = -1;
    }
    errlogFlush();
    return status;
}
