/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* osdEvent.c */
/*
 *      WIN32 version
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 */

#include <limits.h>

#define VC_EXTRALEAN
#define STRICT
#include <windows.h>

#define epicsExportSharedSymbols
#include "shareLib.h"
#include "epicsEvent.h"
#include "epicsAssert.h"

typedef struct epicsEventOSD {
    HANDLE handle;
} epicsEventOSD;

/*
 * epicsEventCreate ()
 */
epicsShareFunc epicsEventId epicsShareAPI epicsEventCreate (
    epicsEventInitialState initialState ) 
{
    epicsEventOSD *pSem;

    pSem = malloc ( sizeof ( *pSem ) );
    if ( pSem ) {
        pSem->handle =  CreateEvent ( NULL, FALSE, initialState?TRUE:FALSE, NULL );
        if ( pSem->handle == 0 ) {
            free ( pSem );
            pSem = 0;
        }
    }

    return pSem;
}

/*
 * epicsEventMustCreate ()
 */
epicsShareFunc epicsEventId epicsShareAPI epicsEventMustCreate (
    epicsEventInitialState initialState ) 
{
    epicsEventId id = epicsEventCreate ( initialState );
    assert ( id );
    return id;
}

/*
 * epicsEventDestroy ()
 */
epicsShareFunc void epicsShareAPI epicsEventDestroy ( epicsEventId pSem ) 
{
    CloseHandle ( pSem->handle );
    free ( pSem );
}

/*
 * epicsEventSignal ()
 */
epicsShareFunc void epicsShareAPI epicsEventSignal ( epicsEventId pSem ) 
{
    BOOL status;
    status = SetEvent ( pSem->handle );
    assert ( status ); 
}

/*
 * epicsEventWait ()
 */
epicsShareFunc epicsEventWaitStatus epicsShareAPI epicsEventWait ( epicsEventId pSem ) 
{ 
    DWORD status;
    status = WaitForSingleObject (pSem->handle, INFINITE);
    if ( status == WAIT_OBJECT_0 ) {
        return epicsEventWaitOK;
    }
    else {
        return epicsEventWaitError;
    }
}

/*
 * epicsEventWaitWithTimeout ()
 */
epicsShareFunc epicsEventWaitStatus epicsShareAPI epicsEventWaitWithTimeout (
    epicsEventId pSem, double timeOut )
{ 
    static const unsigned mSecPerSec = 1000;
    DWORD status;
    DWORD tmo;

    if ( timeOut <= 0.0 ) {
        tmo = 0u;
    }
    else if ( timeOut >= INFINITE / mSecPerSec ) {
        tmo = INFINITE - 1;
    }
    else {
        tmo = ( DWORD ) ( ( timeOut * mSecPerSec ) + 0.5 );
        if ( tmo == 0 ) {
            tmo = 1;
        }
    }
    status = WaitForSingleObject ( pSem->handle, tmo );
    if ( status == WAIT_OBJECT_0 ) {
        return epicsEventWaitOK;
    }
    else if ( status == WAIT_TIMEOUT ) {
        return epicsEventWaitTimeout;
    }
    else {
        return epicsEventWaitError;
    }
}

/*
 * epicsEventTryWait ()
 */
epicsShareFunc epicsEventWaitStatus epicsShareAPI epicsEventTryWait ( epicsEventId pSem ) 
{ 
    DWORD status;

    status = WaitForSingleObject ( pSem->handle, 0 );
    if ( status == WAIT_OBJECT_0 ) {
        return epicsEventWaitOK;
    }
    else if ( status == WAIT_TIMEOUT ) {
        return epicsEventWaitTimeout;
    }
    else {
        return epicsEventWaitError;
    }
}

/*
 * epicsEventShow ()
 */
epicsShareFunc void epicsShareAPI epicsEventShow ( epicsEventId id, unsigned level ) 
{ 
}
