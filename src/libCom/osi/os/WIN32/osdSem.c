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

#ifndef VC_EXTRALEAN
#   define VC_EXTRALEAN
#endif
#ifndef WIN32_LEAN_AND_MEAN 
#   define WIN32_LEAN_AND_MEAN 
#endif
/* including less than this causes conflicts with winsock2.h :-( */
#include <winsock2.h>

#define epicsExportSharedSymbols
#include "shareLib.h"
#include "osiSem.h"
#include "epicsAssert.h"
#include "cantProceed.h"

static const unsigned mSecPerSecOsdSem = 1000u;

typedef struct binarySem {
    HANDLE handle;
}binarySem;

typedef struct mutexSem {
    HANDLE handle;
}mutexSem;

/*
 * semBinaryCreate ()
 */
epicsShareFunc semBinaryId epicsShareAPI semBinaryCreate (int initialState) 
{
    binarySem *pSem;

    pSem = malloc ( sizeof (*pSem) );
    if (pSem) {
        pSem->handle =  CreateEvent (NULL, FALSE, initialState?TRUE:FALSE, NULL);
        if (pSem->handle==0) {
            free (pSem);
            pSem = 0;
        }
    }

    return (semBinaryId) pSem;
}

/*
 * semBinaryMustCreate ()
 */
epicsShareFunc semBinaryId epicsShareAPI semBinaryMustCreate (int initialState) 
{
    semBinaryId id = semBinaryCreate (initialState);
    assert (id);
    return id;
}

/*
 * semBinaryDestroy ()
 */
epicsShareFunc void epicsShareAPI semBinaryDestroy (semBinaryId id) 
{
    binarySem *pSem = (binarySem *) id;
    
    CloseHandle (pSem->handle);
    free (pSem);
}

/*
 * semBinaryGive ()
 */
epicsShareFunc void epicsShareAPI semBinaryGive (semBinaryId id) 
{
    binarySem *pSem = (binarySem *) id;
    BOOL status;

    status = SetEvent (pSem->handle);
    assert (status); 
}

/*
 * semBinaryTake ()
 */
epicsShareFunc semTakeStatus epicsShareAPI semBinaryTake (semBinaryId id) 
{ 
    binarySem *pSem = (binarySem *) id;
    DWORD status;

    status = WaitForSingleObject (pSem->handle, INFINITE);
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
epicsShareFunc semTakeStatus epicsShareAPI semBinaryTakeTimeout (semBinaryId id, double timeOut)
{ 
    binarySem *pSem = (binarySem *) id;
    DWORD status;
    DWORD tmo;

    tmo = (DWORD) (timeOut * mSecPerSecOsdSem);
    status = WaitForSingleObject (pSem->handle, tmo);
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
epicsShareFunc semTakeStatus epicsShareAPI semBinaryTakeNoWait (semBinaryId id) 
{ 
    binarySem *pSem = (binarySem *) id;
    DWORD status;

    status = WaitForSingleObject (pSem->handle, 0);
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
 * semBinaryShow ()
 */
epicsShareFunc void epicsShareAPI semBinaryShow (semBinaryId id, unsigned level) 
{ 
}


/*
 * semMutexCreate ()
 */
epicsShareFunc semMutexId epicsShareAPI semMutexCreate (void) 
{
    mutexSem *pSem;

    pSem = malloc ( sizeof (*pSem) );
    if (pSem) {
        pSem->handle = CreateMutex (NULL, FALSE, NULL);
        if (pSem->handle==0) {
            free (pSem);
            pSem = 0;
        }
    }    

    return (semMutexId) pSem;
}

/*
 * semMutexMustCreate ()
 */
epicsShareFunc semBinaryId epicsShareAPI semMutexMustCreate () 
{
    semMutexId id = semMutexCreate ();
    assert (id);
    return id;
}

/*
 * semMutexDestroy ()
 */
epicsShareFunc void epicsShareAPI semMutexDestroy (semMutexId id) 
{
    mutexSem *pSem = (mutexSem *) id;
    
    CloseHandle (pSem->handle);
    free (pSem);
}

/*
 * semMutexGive ()
 */
epicsShareFunc void epicsShareAPI semMutexGive (semMutexId id) 
{
    mutexSem *pSem = (mutexSem *) id;
    BOOL success;
    
    success = ReleaseMutex (pSem->handle);
    assert (success);
}

/*
 * semMutexTake ()
 */
epicsShareFunc semTakeStatus epicsShareAPI semMutexTake (semMutexId id) 
{
    mutexSem *pSem = (mutexSem *) id;
    DWORD status;

    status = WaitForSingleObject (pSem->handle, INFINITE);
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
epicsShareFunc semTakeStatus epicsShareAPI semMutexTakeTimeout (semMutexId id, double timeOut)
{ 
    mutexSem *pSem = (mutexSem *) id;
    DWORD status;
    DWORD tmo;

    tmo = (DWORD) (timeOut * mSecPerSecOsdSem);
    status = WaitForSingleObject (pSem->handle, tmo);
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
epicsShareFunc semTakeStatus epicsShareAPI semMutexTakeNoWait (semMutexId id) 
{ 
    mutexSem *pSem = (mutexSem *) id;
    DWORD status;

    status = WaitForSingleObject (pSem->handle, 0);
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
 * semMutexShow ()
 */
epicsShareFunc void epicsShareAPI semMutexShow (semMutexId id, unsigned level) 
{ 
}
