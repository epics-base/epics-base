/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <memLib.h>
#include <limits.h>

#define epicsExportSharedSymbols
#include "epicsThread.h"
#include "osiPoolStatus.h"

/* 
 * It turns out that memPartInfoGet() nad memFindMax() are very CPU intensive on vxWorks
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
    int temp = memFindMax ();
    if ( temp > 0 ) {
        osdMaxBlockSize = (size_t) temp;
    }
    else {
        osdMaxBlockSize = 0;
    }
}

static void osdSufficentSpaceInPoolPoll ( void *pArgIn )
{
    while ( 1 ) {
        osdSufficentSpaceInPoolQuery ();
        epicsThreadSleep ( 1.0 );
    }
}

static void osdSufficentSpaceInPoolInit ( void *pArgIn )
{
    epicsThreadId id;

    osdSufficentSpaceInPoolQuery ();

    id = epicsShareAPI epicsThreadCreate ( "poolPoll", epicsThreadPriorityMedium, 
        epicsThreadGetStackSize ( epicsThreadStackSmall ), osdSufficentSpaceInPoolPoll, 0 );
    if ( id ) {
        osdMaxBlockOnceler = 1;
    }
    else {
        epicsThreadSleep ( 0.1 );
    }
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
