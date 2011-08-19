
/*************************************************************************\
* Copyright (c) 2011 LANS LLC, as Operator of 
*     Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 *
 *  Provide a global mutex version of AtomicGuard on POSIX
 *  systems for when we dont have more efficent compiler or 
 *  OS primitives intriniscs to use instead.
 *
 *  We implement this mutex-based AtomicGuard primitive directly 
 *  upon the standalone POSIX pthread library so that the epicsAtomic 
 *  library can be used to implement other primitives such 
 *  epicsThreadOnce.
 *
 *  We use a static initialized pthread mutex to minimize code 
 *  size, and are also optimistic that this can be more efficent 
 *  than pthread_once.
 */

#include <unistd.h>

#define epicsExportSharedSymbols
#include "epicsAtomic.h"

// If we have an inline implementation then implement 
// nothing that conflicts here
#if ! defined ( OSD_ATOMIC_INLINE_DEFINITION )

// get the interface for class AtomicGuard
#include "epicsAtomicGuard.h"

namespace {

// a statically initialized mutex doesnt need to be destroyed 
static pthread_mutex_t AtomicGuard :: mutex = PTHREAD_MUTEX_INITIALIZER;

inline AtomicGuard :: AtomicGuard () throw ()
{
    unsigned countDown = 1000u;
    while ( true ) {
        status = pthread_mutex_lock ( & mutex );
        if ( status != EINTR ) break;
        static const useconds_t retryDelayUSec = 100000;
        usleep ( retryDelayUSec );
        countDown--;
        assert ( countDown );
    }
    assert ( status == 0 );
}

inline AtomicGuard :: ~AtomicGuard () throw ()
{
    const int status = pthread_mutex_unlock ( & mutex );
    assert ( status == 0 );
}

} // end of namespace anonymous

// Define the epicsAtomicXxxx functions out-of-line using this 
// implementation of AtomicGuard. Note that this isnt a traditional 
// c++ header file.
#include "epicsAtomicLocked.h"

#endif // if ! defined ( #define OSD_ATOMIC_INLINE_DEFINITION )
