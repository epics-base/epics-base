/* osi/default/osdInterrupt.c */

/* Author:  Marty Kraimer Date:    15JUL99 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

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

