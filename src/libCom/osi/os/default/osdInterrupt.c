/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* osi/default/osdInterrupt.c */

/* Author:  Marty Kraimer Date:    15JUL99 */

/* This is a version that does not allow osi calls at interrupt level */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#define epicsExportSharedSymbols
#include "epicsMutex.h"
#include "cantProceed.h"
#include "errlog.h"
#include "epicsInterrupt.h"

static epicsMutexId globalLock=0;
static int firstTime = 1;

epicsShareFunc int epicsShareAPI epicsInterruptLock()
{
    if(firstTime) {
        globalLock = epicsMutexMustCreate();
        firstTime = 0;
    }
    epicsMutexMustLock(globalLock);
    return(0);
}

epicsShareFunc void epicsShareAPI epicsInterruptUnlock(int key)
{
    if(firstTime) cantProceed(
        "epicsInterruptUnlock called before epicsInterruptLock\n");
    epicsMutexUnlock(globalLock);
}

epicsShareFunc int epicsShareAPI epicsInterruptIsInterruptContext() { return(0);}

epicsShareFunc void epicsShareAPI epicsInterruptContextMessage(const char *message)
{
    errlogPrintf("%s",message);
}

