/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* Author:  Marty Kraimer Date:    04JAN99 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>

#define epicsExportSharedSymbols
#include "errlog.h"
#include "cantProceed.h"
#include "epicsThread.h"
#include "epicsStackTrace.h"

epicsShareFunc void * callocMustSucceed(size_t count, size_t size, const char *msg)
{
    void * mem = NULL;
    if (count > 0 && size > 0) {
        while ((mem = calloc(count, size)) == NULL) {
            errlogPrintf("%s: callocMustSucceed(%lu, %lu) - calloc failed\n",
                    msg, (unsigned long)count, (unsigned long)size);
            errlogPrintf("Thread %s (%p) suspending.\n",
                    epicsThreadGetNameSelf(), (void *)epicsThreadGetIdSelf());
            errlogFlush();
            epicsThreadSuspendSelf();
        }
    }
    return mem;
}

epicsShareFunc void * mallocMustSucceed(size_t size, const char *msg)
{
    void * mem = NULL;
    if (size > 0) {
        while ((mem = malloc(size)) == NULL) {
            errlogPrintf("%s: mallocMustSucceed(%lu) - malloc failed\n",
                msg, (unsigned long)size);
            errlogPrintf("Thread %s (%p) suspending.\n",
                    epicsThreadGetNameSelf(), (void *)epicsThreadGetIdSelf());
            errlogFlush();
            epicsThreadSuspendSelf();
        }
    }
    return mem;
}

epicsShareFunc void cantProceed(const char *msg, ...)
{
    va_list pvar;
    va_start(pvar, msg);
    if (msg)
        errlogVprintf(msg, pvar);
    va_end(pvar);
    
    errlogPrintf("Thread %s (%p) can't proceed, suspending.\n",
            epicsThreadGetNameSelf(), (void *)epicsThreadGetIdSelf());

    epicsStackTrace();

    errlogFlush();
    
    epicsThreadSleep(1.0);
    while (1)
        epicsThreadSuspendSelf();
}
