/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <memLib.h>
#include <limits.h>

#define epicsExportSharedSymbols
#include "epicsThread.h"
#include "osiPoolStatus.h"

/* 
 * It turns out that memPartInfoGet() and memFindMax() are very CPU intensive on vxWorks
 * so we must spawn off a thread that periodically polls. Although this isnt 100% safe, I 
 * dont see what else to do.
 *
 * It takes about 30 uS to call memPartInfoGet() on a pcPentium I vxWorks system.
 *
 * joh
 */

static epicsThreadOnceId osdMaxBlockOnceler = EPICS_THREAD_ONCE_INIT;

static size_t osdMaxBlockSize = 0;

static void osdSufficentSpaceInPoolQuery ()
{
    osdMaxBlockSize = (size_t) memFindMax ();
}

static void osdSufficentSpaceInPoolPoll ( void *pArgIn )
{
    while ( 1 ) {
        epicsThreadSleep ( 1.0 );
        osdSufficentSpaceInPoolQuery ();
    }
}

static void osdSufficentSpaceInPoolInit ( void *pArgIn )
{
    epicsThreadId id;

    osdSufficentSpaceInPoolQuery ();

    id = epicsThreadCreate ( "poolPoll", epicsThreadPriorityMedium,
        epicsThreadGetStackSize ( epicsThreadStackSmall ),
        osdSufficentSpaceInPoolPoll, 0 );
}

/*
 * osiSufficentSpaceInPool () 
 */
epicsShareFunc int epicsShareAPI osiSufficentSpaceInPool ( size_t contiguousBlockSize )
{
    epicsThreadOnce ( &osdMaxBlockOnceler, osdSufficentSpaceInPoolInit, 0 );

    if ( UINT_MAX - 100000u >= contiguousBlockSize ) {
        return ( osdMaxBlockSize > 100000 + contiguousBlockSize );
    }
    else {
        return 0;
    }
}
