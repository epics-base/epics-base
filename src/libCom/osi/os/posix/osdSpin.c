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
#include "cantProceed.h"
#include "epicsSpin.h"

/* POSIX spinlocks may be subject to priority inversion
 * and so can't be guaranteed safe in situations where
 * threads have different priorities, and thread
 * preemption can't be disabled.
 */
#if defined(DONT_USE_POSIX_THREAD_PRIORITY_SCHEDULING)
#if defined(_POSIX_SPIN_LOCKS) && (_POSIX_SPIN_LOCKS > 0)
#  define USE_PSPIN
#endif
#endif

#define checkStatus(status,message) \
    if ((status)) { \
        errlogPrintf("epicsSpin %s failed: error %s\n", \
            (message), strerror((status))); \
    }

#ifdef USE_PSPIN

/*
 *  POSIX SPIN LOCKS IMPLEMENTATION
 */

typedef struct epicsSpin {
    pthread_spinlock_t lock;
} epicsSpin;

epicsSpinId epicsSpinCreate(void) {
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
    if (status)
        cantProceed(NULL);
}

int epicsSpinTryLock(epicsSpinId spin) {
    int status;

    status = pthread_spin_trylock(&spin->lock);
    if (status == EBUSY)
        return 1;
    checkStatus(status, "pthread_spin_trylock");
    return status;
}

void epicsSpinUnlock(epicsSpinId spin) {
    int status;

    status = pthread_spin_unlock(&spin->lock);
    checkStatus(status, "pthread_spin_unlock");
}

#else /* USE_PSPIN */

/*
 *  POSIX MUTEX IMPLEMENTATION
 */

typedef struct epicsSpin {
    pthread_mutex_t lock;
} epicsSpin;

epicsSpinId epicsSpinCreate(void) {
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
    if (status)
        cantProceed(NULL);
}

int epicsSpinTryLock(epicsSpinId spin) {
    int status;

    status = pthread_mutex_trylock(&spin->lock);
    if (status == EBUSY)
        return 1;
    checkStatus(status, "pthread_mutex_trylock");
    return status;
}

void epicsSpinUnlock(epicsSpinId spin) {
    int status;

    status = pthread_mutex_unlock(&spin->lock);
    checkStatus(status, "pthread_mutex_unlock");
}

#endif /* USE_PSPIN */


epicsSpinId epicsSpinMustCreate(void)
{
    epicsSpinId ret = epicsSpinCreate();
    if(!ret)
        cantProceed("epicsSpinMustCreate: epicsSpinCreate failed.");
    return ret;
}
