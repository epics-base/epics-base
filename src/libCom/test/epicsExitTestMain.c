/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsExitTestMain.c */

/* Author:  Marty Kraimer Date:    24AUG2004 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "epicsExit.h"
#include "epicsThread.h"

void epicsExitTest(void);


int main(int argc,char *argv[])
{
    epicsExitTest();
    epicsThreadSleep(1.0);
    printf("main calling epicsExit\n");
    epicsExit(0);
    return(0);
}
