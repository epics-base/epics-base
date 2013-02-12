/*************************************************************************\
* Copyright (c) 2012 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2012 ITER Organization.
* Copyright (c) 2013 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Authors:  Ralph Lange <Ralph.Lange@gmx.de>
 *           Andrew Johnson <anj@aps.anl.gov>
 *
 * Based on epicsInterrupt.c (RTEMS implementation) by Eric Norum
 */

/*
 * RTEMS (single CPU): LOCK INTERRUPT
 *
 * CAVEAT:
 * This implementation is for UP architectures only.
 *
 */

#include <stdlib.h>
#include <rtems.h>

#include "epicsSpin.h"

typedef struct epicsSpin {
    rtems_interrupt_level level;
} epicsSpin;

epicsSpinId epicsSpinCreate() {
    return calloc(1, sizeof(epicsSpin));
}

void epicsSpinDestroy(epicsSpinId spin) {
    free(spin);
}

void epicsSpinLock(epicsSpinId spin) {
    rtems_interrupt_disable(spin->level);
}

int epicsSpinTryLock(epicsSpinId spin) {
    epicsSpinLock(spin);
    return 0;
}

void epicsSpinUnlock(epicsSpinId spin) {
    rtems_interrupt_enable(spin->level);
}
