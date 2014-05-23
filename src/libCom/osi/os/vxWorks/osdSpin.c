/*************************************************************************\
* Copyright (c) 2012 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2012 ITER Organization.
* Copyright (c) 2013 Brookhaven Science Assoc. as Operator of Brookhaven
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author:  Ralph Lange <Ralph.Lange@gmx.de>
 *          Michael Davidsaver <mdavidsaver@bnl.gov>
 *
 * Inspired by Linux UP spinlocks implemention
 *   include/linux/spinlock_api_up.h
 */

/*
 * vxWorks (single CPU): LOCK INTERRUPT and DISABLE PREEMPTION
 *
 * CAVEAT:
 * This implementation will not compile on vxWorks SMP architectures.
 * These architectures provide spinlocks, which must be used instead.
 *
 */

#include <stdlib.h>
#include <intLib.h>
#include <taskLib.h>

#include "epicsSpin.h"

typedef struct epicsSpin {
    int key;
    unsigned int locked;
} epicsSpin;

epicsSpinId epicsSpinCreate(void) {
    return calloc(1, sizeof(epicsSpin));
}

epicsSpinId epicsSpinMustCreate(void)
{
    epicsSpinId ret = epicsSpinCreate();
    if(!ret)
        cantProceed("epicsSpinMustCreate fails");
    return ret;
}

void epicsSpinDestroy(epicsSpinId spin) {
    free(spin);
}

void epicsSpinLock(epicsSpinId spin) {
    int key = intLock();
    if(!intContext())
        taskLock();
    assert(!spin->locked);
    spin->key = key;
    spin->locked = 1;
}

int epicsSpinTryLock(epicsSpinId spin) {
    epicsSpinLock(spin);
    return 0;
}

void epicsSpinUnlock(epicsSpinId spin) {
    int key = spin->key;
    assert(spin->locked);
    spin->key = spin->locked = 0;
    intUnlock(key);
    if(!intContext())
        taskUnlock();
}
