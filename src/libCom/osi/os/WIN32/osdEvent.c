/* osdEvent.c */
/*
 *      $Id$
 *      WIN32 version
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 */

#include <limits.h>

#ifndef VC_EXTRALEAN
#   define VC_EXTRALEAN
#endif
#ifndef WIN32_LEAN_AND_MEAN 
#   define WIN32_LEAN_AND_MEAN 
#endif
/* including less than this causes conflicts with winsock2.h :-( */
#define _WIN32_WINNT 0x400
#include <winsock2.h>

#define epicsExportSharedSymbols
#include "shareLib.h"
#include "epicsEvent.h"
#include "epicsAssert.h"
#include "cantProceed.h"

static const unsigned mSecPerSecOsdSem = 1000u;

typedef struct eventSem {
    HANDLE handle;
}eventSem;


/*
 * epicsEventCreate ()
 */
epicsShareFunc epicsEventId epicsShareAPI epicsEventCreate (
    epicsEventInitialState initialState ) 
{
    eventSem *pSem;

    pSem = malloc ( sizeof ( *pSem ) );
    if ( pSem ) {
        pSem->handle =  CreateEvent ( NULL, FALSE, initialState?TRUE:FALSE, NULL );
        if ( pSem->handle == 0 ) {
            free ( pSem );
            pSem = 0;
        }
    }

    return ( epicsEventId ) pSem;
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
epicsShareFunc void epicsShareAPI epicsEventDestroy (epicsEventId id) 
{
    eventSem *pSem = (eventSem *) id;
    
    CloseHandle (pSem->handle);
    free (pSem);
}

/*
 * epicsEventSignal ()
 */
epicsShareFunc void epicsShareAPI epicsEventSignal (epicsEventId id) 
{
    eventSem *pSem = (eventSem *) id;
    BOOL status;

    status = SetEvent (pSem->handle);
    assert (status); 
}

/*
 * epicsEventLock ()
 */
epicsShareFunc epicsEventWaitStatus epicsShareAPI epicsEventWait (epicsEventId id) 
{ 
    eventSem *pSem = (eventSem *) id;
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
    epicsEventId id, double timeOut)
{ 
    eventSem *pSem = (eventSem *) id;
    DWORD status;
    DWORD tmo;

    if ( timeOut >= 0xffffffff / mSecPerSecOsdSem ) {
        tmo = 0xfffffffe;
    }
    else if ( timeOut < 0.0 ) {
        tmo = 0u;
    }
    else {
        tmo = ( DWORD ) ( timeOut * mSecPerSecOsdSem );
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
epicsShareFunc epicsEventWaitStatus epicsShareAPI epicsEventTryWait (epicsEventId id) 
{ 
    eventSem *pSem = (eventSem *) id;
    DWORD status;

    status = WaitForSingleObject (pSem->handle, 0);
    if ( status == WAIT_OBJECT_0 ) {
        return epicsEventWaitOK;
    }
    else if (status == WAIT_TIMEOUT) {
        return epicsEventWaitTimeout;
    }
    else {
        return epicsEventWaitError;
    }
}

/*
 * epicsEventShow ()
 */
epicsShareFunc void epicsShareAPI epicsEventShow (epicsEventId id, unsigned level) 
{ 
}
