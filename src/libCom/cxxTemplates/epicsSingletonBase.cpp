/*
 *  $Id$
 *
 *  Author: Jeffrey O. Hill
 *
 */

#include <new>

#define epicsExportSharedSymbols
#include "epicsMutex.h"
#include "epicsGuard.h"
#include "epicsThread.h"
#include "epicsSingleton.h"

static epicsThreadOnceId epicsSingletonOnceId = EPICS_THREAD_ONCE_INIT;
static epicsMutex *pSingletonBaseMutexEPICS;

static void epicsSingletonCleanup ()
{
    delete pSingletonBaseMutexEPICS;
}

static void epicsSingletonOnce ( void * )
{
    pSingletonBaseMutexEPICS = new epicsMutex;
    atexit ( epicsSingletonCleanup );
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