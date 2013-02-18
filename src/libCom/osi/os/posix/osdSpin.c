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

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define epicsExportSharedSymbols
#include "errlog.h"
#include "epicsSpin.h"

#define checkStatus(status,message) \
    if ((status)) { \
        errlogPrintf("epicsSpin %s failed: error %s\n", \
            (message), strerror((status))); \
    }

#if defined(_POSIX_SPIN_LOCKS) && (_POSIX_SPIN_LOCKS > 1)

/*
 *  POSIX SPIN LOCKS IMPLEMENTATION
 */

typedef struct epicsSpin {
    pthread_spinlock_t lock;
} epicsSpin;

epicsSpinId epicsSpinCreate() {
    epicsSpin *spin;
    int status;

    spin = calloc(1, sizeof(*spin));
    if (!spin)
        goto fail;

    status = pthread_spin_init(&spin->lock, PTHREAD_PROCESS_PRIVATE);
    checkStatus(status, "pthread_spin_init");
    if (status)
        goto fail;

    return spin;

fail:
    free(spin);
    return NULL;
}

void epicsSpinDestroy(epicsSpinId spin) {
    int status;

    status = pthread_spin_destroy(&spin->lock);
    checkStatus(status, "pthread_spin_destroy");

    free(spin);
}

void epicsSpinLock(epicsSpinId spin) {
    int status;

    status = pthread_spin_lock(&spin->lock);
    checkStatus(status, "pthread_spin_lock");
}

int epicsSpinTryLock(epicsSpinId spin) {
    int status;

    status = pthread_spin_trylock(&spin->lock);
    if (status == EBUSY)
        return 1;
    checkStatus(status, "pthread_spin_trylock");
    return 0;
}

void epicsSpinUnlock(epicsSpinId spin) {
    int status;

    status = pthread_spin_unlock(&spin->lock);
    checkStatus(status, "pthread_spin_unlock");
}

#else /* defined(_POSIX_SPIN_LOCKS) && (_POSIX_SPIN_LOCKS > 1) */

/*
 *  POSIX MUTEX IMPLEMENTATION
 */

typedef struct epicsSpin {
    pthread_mutex_t lock;
} epicsSpin;

epicsSpinId epicsSpinCreate() {
    epicsSpin *spin;
    int status;

    spin = calloc(1, sizeof(*spin));
    if (!spin)
        goto fail;

    status = pthread_mutex_init(&spin->lock, NULL);
    checkStatus(status, "pthread_mutex_init");
    if (status)
        goto fail;

    return spin;

fail:
    free(spin);
    return NULL;
}

void epicsSpinDestroy(epicsSpinId spin) {
    int status;

    status = pthread_mutex_destroy(&spin->lock);
    checkStatus(status, "pthread_mutex_destroy");

    free(spin);
}

void epicsSpinLock(epicsSpinId spin) {
    int status;

    status = pthread_mutex_lock(&spin->lock);
    checkStatus(status, "pthread_mutex_lock");
}

int epicsSpinTryLock(epicsSpinId spin) {
    int status;

    status = pthread_mutex_trylock(&spin->lock);
    if (status == EBUSY)
        return 1;
    checkStatus(status, "pthread_mutex_trylock");
    return 0;
}

void epicsSpinUnlock(epicsSpinId spin) {
    int status;

    status = pthread_mutex_unlock(&spin->lock);
    checkStatus(status, "pthread_mutex_unlock");
}

#endif /* defined(_POSIX_SPIN_LOCKS) && (_POSIX_SPIN_LOCKS > 1) */
