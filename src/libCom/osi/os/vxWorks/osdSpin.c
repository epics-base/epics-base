/*************************************************************************\
* Copyright (c) 2012 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2012 ITER Organization.
* Copyright (c) 2013 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2013 Brookhaven Science Assoc. as Operator of Brookhaven
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Authors: Ralph Lange <Ralph.Lange@gmx.de>
 *          Andrew Johnson <anj@aps.anl.gov>
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

/* This is needed for vxWorks 6.x to prevent an obnoxious compiler warning */
#define _VSB_CONFIG_FILE <../lib/h/config/vsbConfig.h>

#include <stdlib.h>
#include <intLib.h>
#include <logLib.h>
#include <taskLib.h>

#include "cantProceed.h"
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
    if (!ret)
        cantProceed("epicsSpinMustCreate: epicsSpinCreate failed.");
    return ret;
}

void epicsSpinDestroy(epicsSpinId spin) {
    free(spin);
}

void epicsSpinLock(epicsSpinId spin) {
    int key = intLock();

    if (!intContext())
        taskLock();
    if (spin->locked) {
        intUnlock(key);
        if (!intContext()) {
            taskUnlock();
            logMsg("epicsSpinLock(%p): Deadlock.\n",
                (int) spin, 0, 0, 0, 0, 0);
            cantProceed("Recursive lock, missed unlock or block when locked.");
        }
        else {
            logMsg("epicsSpinLock(%p): Deadlock in ISR.\n"
                "Recursive lock, missed unlock or block when locked.\n",
                (int) spin, 0, 0, 0, 0, 0);
        }
        return;
    }
    spin->key = key;
    spin->locked = 1;
}

int epicsSpinTryLock(epicsSpinId spin) {
    int key = intLock();

    if (!intContext())
        taskLock();
    if (spin->locked) {
        intUnlock(key);
        if (!intContext())
            taskUnlock();
        return 1;
    }
    spin->key = key;
    spin->locked = 1;
    return 0;
}

void epicsSpinUnlock(epicsSpinId spin) {
    int key = spin->key;

    if (!spin->locked) {
        logMsg("epicsSpinUnlock(%p): not locked\n",
            (int) spin, 0, 0, 0, 0, 0);
        return;
    }
    spin->key = spin->locked = 0;
    intUnlock(key);
    if (!intContext())
        taskUnlock();
}
