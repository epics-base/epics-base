/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$ */

/* Author:  Marty Kraimer Date:    04JAN99 */

#include <stddef.h>
#include <stdlib.h>

#define epicsExportSharedSymbols
#include "errlog.h"
#include "cantProceed.h"
#include "epicsThread.h"

epicsShareFunc void * callocMustSucceed(size_t count, size_t size, const char *errorMessage)
{
    void * mem = NULL;
    if (count != 0 && size != 0)
        mem = calloc(count, size);
    if (mem == NULL) {
        errlogPrintf("%s: callocMustSucceed(count=%lu, size=%lu) failed\n",
            errorMessage, (unsigned long)count, (unsigned long)size);
        cantProceed(0);
    }
    return mem;
}

epicsShareFunc void * mallocMustSucceed(size_t size, const char *errorMessage)
{
    void * mem = NULL;
    if (size != 0)
        mem = malloc(size);
    if (mem == NULL) {
        errlogPrintf("%s: mallocMustSucceed(size=%lu) failed\n",
            errorMessage, (unsigned long)size);
        cantProceed(0);
    }
    return mem;
}

epicsShareFunc void cantProceed(const char *errorMessage)
{
    if (errorMessage)
        errlogPrintf("fatal error: %s\n",errorMessage);
    else
        errlogPrintf("fatal error, task suspended\n");
    errlogFlush();
    epicsThreadSleep(1.0);
    while (1)
        epicsThreadSuspendSelf();
}
