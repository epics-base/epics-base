/*************************************************************************\
* Copyright (c) 2012 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2012 ITER Organization.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author:  Ralph Lange <Ralph.Lange@gmx.de>
 *
 * based on epicsInterrupt.c (vxWorks implementation) by Marty Kraimer
 */

/*
 * vxWorks (single CPU): LOCK INTERRUPT
 *
 * CAVEAT:
 * This implementation will not compile on vxWorks SMP architectures.
 * These architectures provide spinlocks, which must be used instead.
 *
 */

#include <stdlib.h>
#include <intLib.h>

#include "epicsSpin.h"

typedef struct epicsSpin {
    int key;
} epicsSpin;

epicsSpinId epicsSpinCreate() {
    return calloc(1, sizeof(epicsSpin));
}

void epicsSpinDestroy(epicsSpinId spin) {
    free(spin);
}

void epicsSpinLock(epicsSpinId spin) {
    spin->key = intLock();
}

int epicsSpinTryLock(epicsSpinId spin) {
    epicsSpinLock(spin);
    return 0;
}

void epicsSpinUnlock(epicsSpinId spin) {
    intUnlock(spin->key);
}
