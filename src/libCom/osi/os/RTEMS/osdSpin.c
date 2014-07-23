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
 * Authors:  Ralph Lange <Ralph.Lange@gmx.de>
 *           Andrew Johnson <anj@aps.anl.gov>
 *           Michael Davidsaver <mdavidsaver@bnl.gov>
 *
 * Inspired by Linux UP spinlocks implemention
 *   include/linux/spinlock_api_up.h
 */

/*
 * RTEMS (single CPU): LOCK INTERRUPT
 *
 * CAVEAT:
 * This implementation is intended for UP architectures only.
 */

#define __RTEMS_VIOLATE_KERNEL_VISIBILITY__ 1

#include <stdlib.h>
#include <rtems.h>
#include <cantProceed.h>

#include "epicsSpin.h"

typedef struct epicsSpin {
    rtems_interrupt_level level;
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
    rtems_interrupt_level level;
    rtems_interrupt_disable (level);
    _Thread_Disable_dispatch();
    spin->level = level;
    if(spin->locked) {
        rtems_interrupt_enable (level);
        _Thread_Enable_dispatch();
        if(!rtems_interrupt_is_in_progress()) {
            printk("Deadlock in epicsSpinLock(%p).  Either recursive lock, missing unlock, or locked by sleeping thread.", spin);
            cantProceed(NULL);
        } else {
            printk("epicsSpinLock(%p) failure in ISR.  Either recursive lock, missing unlock, or locked by sleeping thread.\n", spin);
        }
        return;
    }
    spin->locked = 1;
}

int epicsSpinTryLock(epicsSpinId spin) {
    epicsSpinLock(spin);
    return 0;
}

void epicsSpinUnlock(epicsSpinId spin) {
    rtems_interrupt_level level = spin->level;
    if(!spin->locked) {
        printk("epicsSpinUnlock(%p) failure.  lock not taken\n", spin);
        return;
    }
    spin->level = spin->locked = 0;
    rtems_interrupt_enable (level);
    _Thread_Enable_dispatch();
}
