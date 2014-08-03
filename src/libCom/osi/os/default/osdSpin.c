/*************************************************************************\
* Copyright (c) 2012 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2012 ITER Organization.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author:  Ralph Lange <Ralph.Lange@gmx.de>
 */

#include <stdlib.h>

#define epicsExportSharedSymbols
#include "cantProceed.h"
#include "errlog.h"
#include "epicsMutex.h"
#include "epicsSpin.h"

/*
 *  Default: EPICS MUTEX IMPLEMENTATION
 */

typedef struct epicsSpin {
    epicsMutexId lock;
} epicsSpin;

epicsSpinId epicsSpinCreate(void) {
    epicsSpin *spin;

    spin = calloc(1, sizeof(*spin));
    if (!spin)
        goto fail;

    spin->lock = epicsMutexCreate();
    if (!spin->lock)
        goto fail;

    return spin;

fail:
    free(spin);
    return NULL;
}

epicsSpinId epicsSpinMustCreate(void)
{
    epicsSpinId ret = epicsSpinCreate();
    if(!ret)
        cantProceed("epicsSpinMustCreate: epicsSpinCreate failed.");
    return ret;
}

void epicsSpinDestroy(epicsSpinId spin) {
    epicsMutexDestroy(spin->lock);
    free(spin);
}

void epicsSpinLock(epicsSpinId spin) {
    epicsMutexLockStatus status;

    status = epicsMutexLock(spin->lock);
    if (status != epicsMutexLockOK) {
        errlogPrintf("epicsSpinLock(%p): epicsMutexLock returned %s\n", spin, 
                     status == epicsMutexLockTimeout ?
                         "epicsMutexLockTimeout" : "epicsMutexLockError");
    }
}

int epicsSpinTryLock(epicsSpinId spin) {
    epicsMutexLockStatus status;

    status = epicsMutexTryLock(spin->lock);
    if (status == epicsMutexLockOK) return 0;
    if (status == epicsMutexLockTimeout) return 1;

    errlogPrintf("epicsSpinTryLock(%p): epicsMutexTryLock returned epicsMutexLockError\n", spin);
    return 2;
}

void epicsSpinUnlock(epicsSpinId spin) {
    epicsMutexUnlock(spin->lock);
}
