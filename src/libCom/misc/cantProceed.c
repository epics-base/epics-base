/* callocMustSucceed.c */

/* Author:  Marty Kraimer Date:    04JAN99 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#define epicsExportSharedSymbols
#include "errlog.h"
#include "cantProceed.h"
#include "osiThread.h"

epicsShareFunc void * epicsShareAPI callocMustSucceed(size_t count, size_t size, const char *errorMessage)
{
    void *mem = calloc(count,size);
    if(mem==0) {
        errlogPrintf("%s callocMustSucceed failed count %d size %d\n",
            errorMessage,count,size);
        cantProceed(0);
    }
    return(mem);
}

epicsShareFunc void * epicsShareAPI mallocMustSucceed(size_t size, const char *errorMessage)
{
    void *mem = malloc(size);
    if(mem==0) {
        errlogPrintf("%s mallocMustSucceed failed size %d\n",
            errorMessage,size);
        cantProceed(0);
    }
    return(mem);
}

epicsShareFunc void epicsShareAPI cantProceed(const char *errorMessage)
{
    if(errorMessage) errlogPrintf("fatal error: %s\n",errorMessage);
    else errlogPrintf("fatal error\n");
    threadSleep(1.0);
    threadSuspendSelf();
}
