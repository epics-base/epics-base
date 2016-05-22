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
 *  Author: Jeff O. Hill
 */

#include <climits>

#define epicsExportSharedSymbols
#include "epicsMutex.h"
#include "epicsGuard.h"
#include "epicsThread.h"
#include "epicsSingleton.h"

#ifndef SIZE_MAX
#   define SIZE_MAX UINT_MAX
#endif

static epicsThreadOnceId epicsSigletonOnceFlag ( EPICS_THREAD_ONCE_INIT );
static epicsMutex * pEPICSSigletonMutex = 0;

extern "C" void SingletonMutexOnce ( void * /* pParm */ )
{
    // This class exists for the purpose of avoiding file scope
    // object chicken and egg problems. Therefore, pEPICSSigletonMutex 
    // is never destroyed.
    pEPICSSigletonMutex = newEpicsMutex;
}

void SingletonUntyped :: incrRefCount ( PBuild pBuild )
{
    epicsThreadOnce ( & epicsSigletonOnceFlag, SingletonMutexOnce, 0 );
    epicsGuard < epicsMutex > 
        guard ( *pEPICSSigletonMutex );
    assert ( _refCount < SIZE_MAX );
    if ( _refCount == 0 ) {
        _pInstance = ( * pBuild ) ();
    }
    _refCount++;
}

void SingletonUntyped :: decrRefCount ( PDestroy pDestroy )
{
    epicsGuard < epicsMutex > 
        guard ( *pEPICSSigletonMutex );
    assert ( _refCount > 0 );
    _refCount--;
    if ( _refCount == 0 ) {
        ( *pDestroy ) ( _pInstance );
        _pInstance = 0;
    }
}
