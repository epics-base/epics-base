/* osi/osiInterrupt.c */

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
#include "osiSem.h"
#include "cantProceed.h"
#include "errlog.h"
#include "osiInterrupt.h"

static semMutexId globalLock=0;
static int firstTime = 1;

epicsShareFunc int epicsShareAPI interruptLock()
{
    if(firstTime) {
        globalLock = semMutexMustCreate();
        firstTime = 0;
    }
    semMutexMustTake(globalLock);
    return(0);
}

epicsShareFunc void epicsShareAPI interruptUnlock(int key)
{
    if(firstTime) cantProceed("interruptUnlock called before interruptLock\n");
    semMutexGive(globalLock);
}

epicsShareFunc int epicsShareAPI interruptIsInterruptContext() { return(0);}

epicsShareFunc void epicsShareAPI interruptContextMessage(const char *message)
{
    errlogPrintf("%s",message);
}

