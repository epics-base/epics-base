/* osdMutex.c */
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
#include "epicsMutex.h"
#include "epicsAssert.h"
#include "cantProceed.h"

static const unsigned mSecPerSecOsdSem = 1000u;

#if 0

typedef struct mutexSem {
    HANDLE handle;
}mutexSem;


/*
 * epicsMutexCreate ()
 */
epicsShareFunc epicsMutexId epicsShareAPI epicsMutexCreate (void) 
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

    return (epicsMutexId) pSem;
}

/*
 * epicsMutexMustCreate ()
 */
epicsShareFunc epicsMutexId epicsShareAPI epicsMutexMustCreate () 
{
    epicsMutexId id = epicsMutexCreate ();
    assert (id);
    return id;
}

/*
 * epicsMutexDestroy ()
 */
epicsShareFunc void epicsShareAPI epicsMutexDestroy (epicsMutexId id) 
{
    mutexSem *pSem = (mutexSem *) id;
    
    CloseHandle (pSem->handle);
    free (pSem);
}

/*
 * epicsMutexUnlock ()
 */
epicsShareFunc void epicsShareAPI epicsMutexUnlock (epicsMutexId id) 
{
    mutexSem *pSem = (mutexSem *) id;
    BOOL success;
    
    success = ReleaseMutex (pSem->handle);
    assert (success);
}

/*
 * epicsMutexLock ()
 */
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexLock (epicsMutexId id) 
{
    mutexSem *pSem = (mutexSem *) id;
    DWORD status;

    status = WaitForSingleObject (pSem->handle, INFINITE);
    if ( status == WAIT_OBJECT_0 ) {
        return epicsMutexLockOK;
    }
    else {
        return epicsMutexLockError;
    }
}

/*
 * epicsMutexLockWithTimeout ()
 */
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexLockWithTimeout (epicsMutexId id, double timeOut)
{ 
    mutexSem *pSem = (mutexSem *) id;
    DWORD status;
    DWORD tmo;

    tmo = (DWORD) (timeOut * mSecPerSecOsdSem);
    status = WaitForSingleObject (pSem->handle, tmo);
    if ( status == WAIT_OBJECT_0 ) {
        return epicsMutexLockOK;
    }
    else if (status == WAIT_TIMEOUT) {
        return epicsMutexLockTimeout;
    }
    else {
        return epicsMutexLockError;
    }
}

/*
 * epicsMutexTryLock ()
 */
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexTryLock (epicsMutexId id) 
{ 
    mutexSem *pSem = (mutexSem *) id;
    DWORD status;

    status = WaitForSingleObject (pSem->handle, 0);
    if ( status == WAIT_OBJECT_0 ) {
        return epicsMutexLockOK;
    }
    else if (status == WAIT_TIMEOUT) {
        return epicsMutexLockTimeout;
    }
    else {
        return epicsMutexLockError;
    }
}

/*
 * epicsMutexShow ()
 */
epicsShareFunc void epicsShareAPI epicsMutexShow (epicsMutexId id, unsigned level) 
{ 
}

#elif 0

typedef struct mutexSem {
    CRITICAL_SECTION cs;
} mutexSem;

/*
 * epicsMutexCreate ()
 */
epicsShareFunc epicsMutexId epicsShareAPI epicsMutexCreate ( void ) 
{
    mutexSem *pSem;

    pSem = malloc ( sizeof (*pSem) );
    if ( pSem ) {
        InitializeCriticalSection ( &pSem->cs );
    }    

    return (epicsMutexId) pSem;
}

/*
 * epicsMutexMustCreate ()
 */
epicsShareFunc semBinaryId epicsShareAPI epicsMutexMustCreate () 
{
    epicsMutexId id = epicsMutexCreate ();
    assert ( id );
    return id;
}

/*
 * epicsMutexDestroy ()
 */
epicsShareFunc void epicsShareAPI epicsMutexDestroy ( epicsMutexId id ) 
{
    mutexSem *pSem = ( mutexSem * ) id;
    
    DeleteCriticalSection  ( &pSem->cs );
    free ( pSem );
}

/*
 * epicsMutexUnlock ()
 */
epicsShareFunc void epicsShareAPI epicsMutexUnlock ( epicsMutexId id ) 
{
    mutexSem *pSem = ( mutexSem * ) id;
    LeaveCriticalSection ( &pSem->cs );
}

/*
 * epicsMutexLock ()
 */
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexLock ( epicsMutexId id ) 
{
    mutexSem *pSem = ( mutexSem * ) id;
    EnterCriticalSection ( &pSem->cs );
    return epicsMutexLockOK;
}

/*
 * epicsMutexLockWithTimeout ()
 */
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexLockWithTimeout ( epicsMutexId id, double timeOut )
{ 
    mutexSem *pSem = ( mutexSem * ) id;
    EnterCriticalSection ( &pSem->cs );
    return epicsMutexLockOK;
}

/*
 * epicsMutexTryLock ()
 */
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexTryLock ( epicsMutexId id ) 
{ 
    mutexSem *pSem = ( mutexSem * ) id;
    if ( TryEnterCriticalSection ( &pSem->cs ) ) {
        return epicsMutexLockOK;
    }
    else {
        return epicsMutexLockTimeout;
    }
}

/*
 * epicsMutexShow ()
 */
epicsShareFunc void epicsShareAPI epicsMutexShow ( epicsMutexId id, unsigned level ) 
{ 
}

#else

typedef struct mutexSem {
    CRITICAL_SECTION cs;
    DWORD threadId;
    HANDLE unlockSignal;
    unsigned count;
} mutexSem;

/*
 * epicsMutexCreate ()
 */
epicsShareFunc epicsMutexId epicsShareAPI epicsMutexCreate ( void ) 
{
    mutexSem *pSem;

    pSem = malloc ( sizeof (*pSem) );
    if ( pSem ) {
        pSem->unlockSignal = CreateEvent ( NULL, FALSE, FALSE, NULL );
        if ( pSem->unlockSignal == 0 ) {
            free ( pSem );
            pSem = 0;
        }
        else {
            InitializeCriticalSection ( &pSem->cs );
            pSem->threadId = 0;
            pSem->count = 0u;
        }
    }    
    return (epicsMutexId) pSem;
}

/*
 * epicsMutexMustCreate ()
 */
epicsShareFunc epicsMutexId epicsShareAPI epicsMutexMustCreate () 
{
    epicsMutexId id = epicsMutexCreate ();
    assert ( id );
    return id;
}

/*
 * epicsMutexDestroy ()
 */
epicsShareFunc void epicsShareAPI epicsMutexDestroy ( epicsMutexId id ) 
{
    mutexSem *pSem = ( mutexSem * ) id;
    
    DeleteCriticalSection  ( &pSem->cs );
    CloseHandle ( pSem->unlockSignal );
    free ( pSem );
}

/*
 * epicsMutexUnlock ()
 */
epicsShareFunc void epicsShareAPI epicsMutexUnlock ( epicsMutexId id ) 
{
    mutexSem *pSem = ( mutexSem * ) id;
    unsigned signalNeeded;
    DWORD status;

    EnterCriticalSection ( &pSem->cs );
    //assert ( pSem->threadId == GetCurrentThreadId () );
    assert ( pSem->count > 0u );
    pSem->count--;
    if ( pSem->count == 0 ) {
        pSem->threadId = 0;
        signalNeeded = 1;
    }
    else {
        signalNeeded = 0;
    }
    LeaveCriticalSection ( &pSem->cs );

    if ( signalNeeded ) {
        status = SetEvent ( pSem->unlockSignal );
        assert ( status ); 
    }
}

/*
 * epicsMutexLock ()
 */
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexLock ( epicsMutexId id ) 
{
    DWORD thisThread = GetCurrentThreadId ();
    mutexSem *pSem = ( mutexSem * ) id;

    EnterCriticalSection ( &pSem->cs );

    while ( pSem->count && pSem->threadId != thisThread ) {
        DWORD status;

        LeaveCriticalSection ( &pSem->cs );
        status = WaitForSingleObject ( pSem->unlockSignal, INFINITE );
        if ( status == WAIT_TIMEOUT ) {
            return epicsMutexLockTimeout;
        }
        EnterCriticalSection ( &pSem->cs );
    }

    pSem->threadId = thisThread;
    assert ( pSem->count != UINT_MAX );
    pSem->count++;

    LeaveCriticalSection ( &pSem->cs );

    return epicsMutexLockOK;
}

/*
 * epicsMutexLockWithTimeout ()
 */
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexLockWithTimeout ( epicsMutexId id, double timeOut )
{ 
    DWORD thisThread = GetCurrentThreadId ();

    mutexSem *pSem = ( mutexSem * ) id;

    EnterCriticalSection ( &pSem->cs );

    while ( pSem->count && pSem->threadId != thisThread ) {
        DWORD tmo;
        DWORD status;

        LeaveCriticalSection ( &pSem->cs );
        tmo = ( DWORD ) ( timeOut * mSecPerSecOsdSem );
        status = WaitForSingleObject ( pSem->unlockSignal, tmo );
        if ( status == WAIT_TIMEOUT ) {
            return epicsMutexLockTimeout;
        }
        EnterCriticalSection ( &pSem->cs );
    }

    pSem->threadId = thisThread;
    assert ( pSem->count != UINT_MAX );
    pSem->count++;

    LeaveCriticalSection ( &pSem->cs );

    return epicsMutexLockOK;
}

/*
 * epicsMutexTryLock ()
 */
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexTryLock ( epicsMutexId id ) 
{ 
    DWORD thisThread = GetCurrentThreadId ();

    mutexSem *pSem = ( mutexSem * ) id;

    EnterCriticalSection ( &pSem->cs );

    if ( pSem->count && pSem->threadId != thisThread ) {
        LeaveCriticalSection ( &pSem->cs );
        return epicsMutexLockTimeout;
    }

    pSem->threadId = thisThread;
    assert ( pSem->count != UINT_MAX );
    pSem->count++;

    LeaveCriticalSection ( &pSem->cs );

    return epicsMutexLockOK;
}

/*
 * epicsMutexShow ()
 */
epicsShareFunc void epicsShareAPI epicsMutexShow ( epicsMutexId id, unsigned level ) 
{ 
}


#endif

