/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsMutexTestMain.c */

/* Author:  Marty Kraimer Date:    26JAN2000 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "epicsThread.h"
extern "C" void epicsMutexTest(int nthreads,int errVerbose);


int main(int argc,char *argv[])
{
    int nthreads = 2;
    int errVerboseIn = 0;

    if(argc>1) {
        if(isdigit(*argv[1])) {
            sscanf(argv[1],"%d",&nthreads);
            printf("nthreads %d\n",nthreads);
        } else {
            printf("Illegal argument %s\n",argv[1]);
        }
    }
    if(argc>2) {
        if(isdigit(*argv[2])) {
            sscanf(argv[2],"%d",&errVerboseIn);
            printf("errVerbose %d\n",errVerboseIn);
        } else {
            printf("Illegal argument %s\n",argv[1]);
        }
    }
    epicsMutexTest(nthreads,errVerboseIn);
    printf("main terminating\n");
    return(0);
}
