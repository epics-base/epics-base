/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* osi/os/vxWorks/osdInterrupt.c */

/* Author:  Marty Kraimer Date:    28JAN2000 */

#include <vxWorks.h>
#include <intLib.h>
#include <logLib.h>

#include "epicsInterrupt.h"

int  epicsInterruptLock() {return(intLock());}

void  epicsInterruptUnlock(int key) {intUnlock(key);}

int  epicsInterruptIsInterruptContext() {return(intContext());}

void  epicsInterruptContextMessage(const char *message)
{
    logMsg((char *)message,0,0,0,0,0,0);
}
