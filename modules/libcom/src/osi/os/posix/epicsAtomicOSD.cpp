
/*************************************************************************\
* Copyright (c) 2011 LANS LLC, as Operator of 
*     Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *  Author: Jeffrey O. Hill
 *  johill@lanl.gov
 */

#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#define epicsExportSharedSymbols
#include "epicsAssert.h"
#include "epicsAtomic.h"

/*
 * Slow, but probably correct on all systems.
 * Useful only if something more efficient isn`t 
 * provided based on knowledge of the compiler 
 * or OS 
 *
 * A statically initialized pthread mutex doesn`t 
 * need to be destroyed 
 * 
 * !!!!!
 * !!!!! WARNING 
 * !!!!!
 * !!!!! Do not use this implementation on systems where
 * !!!!! code runs at interrupt context. If so, then 
 * !!!!! an implementation must be provided that is based
 * !!!!! on a compiler intrinsic or an interrupt lock and or
 * !!!!! a spin lock primitive
 * !!!!!
 */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void epicsAtomicLock ( EpicsAtomicLockKey * )
{
    unsigned countDown = 1000u;
    int status;
    while ( true ) {
        status = pthread_mutex_lock ( & mutex );
        if ( status == 0 ) return;
        assert ( status == EINTR );
        static const useconds_t retryDelayUSec = 100000;
        usleep ( retryDelayUSec );
        countDown--;
        assert ( countDown );
    }
}

void epicsAtomicUnlock ( EpicsAtomicLockKey * )
{
    const int status = pthread_mutex_unlock ( & mutex );
    assert ( status == 0 );
}


// Slow, but probably correct on all systems.
// Useful only if something more efficient isn`t 
// provided based on knowledge of the compiler 
// or OS 
void epicsAtomicMemoryBarrierFallback (void)
{
    EpicsAtomicLockKey key;
    epicsAtomicLock ( & key  );
    epicsAtomicUnlock ( & key  );
}
