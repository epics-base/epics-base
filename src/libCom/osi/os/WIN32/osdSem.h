
/* osiSem.c */
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

#ifndef osdSemh
#define osdSemh

#ifndef VC_EXTRALEAN
#   define VC_EXTRALEAN
#endif
#ifndef WIN32_LEAN_AND_MEAN 
#   define WIN32_LEAN_AND_MEAN 
#endif
/* including less than this causes conflicts with winsock2.h :-( */
#include <winsock2.h>

#include "epicsAssert.h"
#include "cantProceed.h"

static const unsigned mSecPerSecOsdSem = 1000u;

/*
 * semBinaryCreate ()
 */
epicsShareFunc INLINE semBinaryId epicsShareAPI semBinaryCreate (int initialState) 
{
    HANDLE newEvent;

    newEvent =  CreateEvent (NULL, FALSE, initialState?TRUE:FALSE, NULL);
    return (semBinaryId) newEvent;
}


/*
 * semBinaryDestroy ()
 */
epicsShareFunc INLINE void epicsShareAPI semBinaryDestroy (semBinaryId id) 
{
    HANDLE destroyee = (HANDLE) id;
    BOOL success;
    
    success = CloseHandle (destroyee);
    assert (success);
}

/*
 * semBinaryGive ()
 */
epicsShareFunc INLINE void epicsShareAPI semBinaryGive (semBinaryId id) 
{
    HANDLE event = (HANDLE) id;
    BOOL status;

    status = SetEvent (event);
    assert (status); 
}

/*
 * semBinaryTake ()
 */
epicsShareFunc INLINE semTakeStatus epicsShareAPI semBinaryTake (semBinaryId id) 
{ 
    HANDLE event = (HANDLE) id;
    DWORD status;

    status = WaitForSingleObject (event, INFINITE);
    if ( status == WAIT_OBJECT_0 ) {
        return semTakeOK;
    }
    else {
        return semTakeError;
    }
}

/*
 * semBinaryTakeTimeout ()
 */
epicsShareFunc INLINE semTakeStatus epicsShareAPI semBinaryTakeTimeout (semBinaryId id, double timeOut)
{ 
    HANDLE event = (HANDLE) id;
    DWORD status;
    DWORD tmo;

    tmo = (DWORD) (timeOut * mSecPerSecOsdSem);
    status = WaitForSingleObject (event, tmo);
    if ( status == WAIT_OBJECT_0 ) {
        return semTakeOK;
    }
    else if (status == WAIT_TIMEOUT) {
        return semTakeTimeout;
    }
    else {
        return semTakeError;
    }
}

/*
 * semBinaryTakeNoWait ()
 */
epicsShareFunc INLINE semTakeStatus epicsShareAPI semBinaryTakeNoWait (semBinaryId id) 
{ 
    HANDLE mutex = (HANDLE) id;
    DWORD status;

    status = WaitForSingleObject (mutex, 0);
    if ( status == WAIT_OBJECT_0 ) {
        return semTakeOK;
    }
    else if (status == WAIT_TIMEOUT) {
        return semTakeTimeout;
    }
    else {
        return semTakeError;
    }
}

/*
 * semMutexCreate ()
 */
epicsShareFunc INLINE semMutexId epicsShareAPI semMutexCreate (void) 
{
    HANDLE newMutex;

    newMutex = CreateMutex (NULL, FALSE, NULL);
    return (semMutexId) newMutex;
}

/*
 * semMutexDestroy ()
 */
epicsShareFunc INLINE void epicsShareAPI semMutexDestroy (semMutexId id) 
{
    HANDLE destroyee = (HANDLE) id;
    BOOL success;
    
    success = CloseHandle (destroyee);
    assert (success);
}

/*
 * semMutexGive ()
 */
epicsShareFunc INLINE void epicsShareAPI semMutexGive (semMutexId id) 
{
    HANDLE destroyee = (HANDLE) id;
    BOOL success;
    
    success = ReleaseMutex (destroyee);
    assert (success);
}

/*
 * semMutexTake ()
 */
epicsShareFunc INLINE semTakeStatus epicsShareAPI semMutexTake (semMutexId id) 
{
    HANDLE mutex = (HANDLE) id;
    DWORD status;

    status = WaitForSingleObject (mutex, INFINITE);
    if ( status == WAIT_OBJECT_0 ) {
        return semTakeOK;
    }
    else {
        return semTakeError;
    }
}

/*
 * semMutexTakeTimeout ()
 */
epicsShareFunc INLINE semTakeStatus epicsShareAPI semMutexTakeTimeout (semMutexId id, double timeOut)
{ 
    HANDLE mutex = (HANDLE) id;
    DWORD status;
    DWORD tmo;

    tmo = (DWORD) (timeOut * mSecPerSecOsdSem);
    status = WaitForSingleObject (mutex, tmo);
    if ( status == WAIT_OBJECT_0 ) {
        return semTakeOK;
    }
    else if (status == WAIT_TIMEOUT) {
        return semTakeTimeout;
    }
    else {
        return semTakeError;
    }
}

/*
 * semMutexTakeNoWait ()
 */
epicsShareFunc INLINE semTakeStatus epicsShareAPI semMutexTakeNoWait (semMutexId id) 
{ 
    HANDLE mutex = (HANDLE) id;
    DWORD status;

    status = WaitForSingleObject (mutex, 0);
    if ( status == WAIT_OBJECT_0 ) {
        return semTakeOK;
    }
    else if (status == WAIT_TIMEOUT) {
        return semTakeTimeout;
    }
    else {
        return semTakeError;
    }
}

#endif /* osdSemh */