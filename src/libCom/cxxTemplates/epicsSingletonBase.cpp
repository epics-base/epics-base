/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *  Author: Jeffrey O. Hill
 */

#include <new>

#define epicsExportSharedSymbols
#include "epicsMutex.h"
#include "epicsGuard.h"
#include "epicsThread.h"
#include "epicsSingleton.h"
#include "epicsExit.h"

static epicsThreadOnceId epicsSingletonOnceId = EPICS_THREAD_ONCE_INIT;
static epicsMutex *pSingletonBaseMutexEPICS;

static void epicsSingletonCleanup (void *)
{
    delete pSingletonBaseMutexEPICS;
}

static void epicsSingletonOnce ( void * )
{
    pSingletonBaseMutexEPICS = newEpicsMutex;
    epicsAtExit ( epicsSingletonCleanup,0 );
}

epicsSingletonBase::epicsSingletonBase () : pSingleton ( 0 )
{
}

void * epicsSingletonBase::singletonPointer () const
{
    return this->pSingleton;
}

epicsSingletonBase::~epicsSingletonBase ()
{
}

void epicsSingletonBase::lockedFactory ()
{
    if ( ! this->pSingleton ) {
        epicsThreadOnce ( & epicsSingletonOnceId, epicsSingletonOnce, 0 );
        epicsGuard < epicsMutex > guard ( *pSingletonBaseMutexEPICS );
        if ( ! this->pSingleton ) {
            this->pSingleton = this->factory ();
        }
    }
}
