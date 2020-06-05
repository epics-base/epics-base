/*************************************************************************\
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
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

#include "libComAPI.h"
#include "epicsEvent.h"

typedef struct epicsEventOSD {
    HANDLE handle;
} epicsEventOSD;

/*
 * epicsEventCreate ()
 */
LIBCOM_API epicsEventId epicsEventCreate (
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
 * epicsEventDestroy ()
 */
LIBCOM_API void epicsEventDestroy ( epicsEventId pSem ) 
{
    CloseHandle ( pSem->handle );
    free ( pSem );
}

/*
 * epicsEventTrigger ()
 */
LIBCOM_API epicsEventStatus epicsEventTrigger ( epicsEventId pSem ) 
{
    BOOL status;
    status = SetEvent ( pSem->handle );
    return status ? epicsEventOK : epicsEventError;
}

/*
 * epicsEventWait ()
 */
LIBCOM_API epicsEventStatus epicsEventWait ( epicsEventId pSem ) 
{
    DWORD status;
    status = WaitForSingleObject (pSem->handle, INFINITE);
    if ( status == WAIT_OBJECT_0 ) {
        return epicsEventOK;
    }
    else {
        return epicsEventError;
    }
}

/*
 * epicsEventWaitWithTimeout ()
 */
LIBCOM_API epicsEventStatus epicsEventWaitWithTimeout (
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
        return epicsEventOK;
    }
    else if ( status == WAIT_TIMEOUT ) {
        return epicsEventWaitTimeout;
    }
    else {
        return epicsEventError;
    }
}

/*
 * epicsEventTryWait ()
 */
LIBCOM_API epicsEventStatus epicsEventTryWait ( epicsEventId pSem ) 
{
    DWORD status;

    status = WaitForSingleObject ( pSem->handle, 0 );
    if ( status == WAIT_OBJECT_0 ) {
        return epicsEventOK;
    }
    else if ( status == WAIT_TIMEOUT ) {
        return epicsEventWaitTimeout;
    }
    else {
        return epicsEventError;
    }
}

/*
 * epicsEventShow ()
 */
LIBCOM_API void epicsEventShow ( epicsEventId id, unsigned level ) 
{
}
