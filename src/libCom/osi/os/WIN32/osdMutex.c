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

#include <stdio.h>
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

#if 0

typedef struct epicsMutexOSD {
    HANDLE handle;
}epicsMutexOSD;


/*
 * epicsMutexOsdCreate ()
 */
epicsShareFunc epicsMutexId epicsShareAPI epicsMutexOsdCreate (void) 
{
    epicsMutexOSD *pSem;

    pSem = malloc ( sizeof (*pSem) );
    if (pSem) {
        pSem->handle = CreateMutex (NULL, FALSE, NULL);
        if (pSem->handle==0) {
            free (pSem);
            pSem = 0;
        }
    }    

    return pSem;
}

/*
 * epicsMutexOsdDestroy ()
 */
epicsShareFunc void epicsShareAPI epicsMutexOsdDestroy (epicsMutexId pSem) 
{
    CloseHandle (pSem->handle);
    free (pSem);
}

/*
 * epicsMutexUnlock ()
 */
epicsShareFunc void epicsShareAPI epicsMutexUnlock (epicsMutexId pSem) 
{
    BOOL success;
    success = ReleaseMutex (pSem->handle);
    assert (success);
}

/*
 * epicsMutexLock ()
 */
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexLock (epicsMutexId pSem) 
{
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
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexLockWithTimeout (epicsMutexId pSem, double timeOut)
{ 
    static const unsigned mSecPerSec = 1000u;
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
        return epicsMutexLockOK;
    }
    else if ( status == WAIT_TIMEOUT ) {
        return epicsMutexLockTimeout;
    }
    else {
        return epicsMutexLockError;
    }
}

/*
 * epicsMutexTryLock ()
 */
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexTryLock (epicsMutexId pSem) 
{ 
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
epicsShareFunc void epicsShareAPI epicsMutexShow (epicsMutexId pSem, unsigned level) 
{ 
}

#elif 0

typedef struct epicsMutexOSD {
    CRITICAL_SECTION cs;
} epicsMutexOSD;

/*
 * epicsMutexCreate ()
 */
epicsShareFunc epicsMutexId epicsShareAPI epicsMutexOsdCreate ( void ) 
{
    epicsMutexOSD *pSem;

    pSem = malloc ( sizeof (*pSem) );
    if ( pSem ) {
        InitializeCriticalSection ( &pSem->cs );
    }    

    return pSem;
}

/*
 * epicsMutexOsdDestroy ()
 */
epicsShareFunc void epicsShareAPI epicsMutexOsdDestroy ( epicsMutexId pSem ) 
{
    DeleteCriticalSection  ( &pSem->cs );
    free ( pSem );
}

/*
 * epicsMutexUnlock ()
 */
epicsShareFunc void epicsShareAPI epicsMutexUnlock ( epicsMutexId pSem ) 
{
    LeaveCriticalSection ( &pSem->cs );
}

/*
 * epicsMutexLock ()
 */
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexLock ( epicsMutexId pSem ) 
{
    EnterCriticalSection ( &pSem->cs );
    return epicsMutexLockOK;
}

/*
 * epicsMutexLockWithTimeout ()
 */
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexLockWithTimeout ( epicsMutexId pSem, double timeOut )
{ 
    EnterCriticalSection ( &pSem->cs );
    return epicsMutexLockOK;
}

/*
 * epicsMutexTryLock ()
 */
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexTryLock ( epicsMutexId pSem ) 
{ 
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
epicsShareFunc void epicsShareAPI epicsMutexShow ( epicsMutexId pSem, unsigned level ) 
{ 
}

#else

typedef struct epicsMutexOSD {
    CRITICAL_SECTION cs;
    DWORD threadId;
    HANDLE unlockSignal;
    unsigned count;
} epicsMutexOSD;

/*
 * epicsMutexCreate ()
 */
epicsShareFunc epicsMutexId epicsShareAPI epicsMutexOsdCreate ( void ) 
{
    epicsMutexOSD *pSem;

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
    return pSem;
}

/*
 * epicsMutexOsdDestroy ()
 */
epicsShareFunc void epicsShareAPI epicsMutexOsdDestroy ( epicsMutexId pSem ) 
{    
    DeleteCriticalSection  ( &pSem->cs );
    CloseHandle ( pSem->unlockSignal );
    free ( pSem );
}

/*
 * epicsMutexUnlock ()
 */
epicsShareFunc void epicsShareAPI epicsMutexUnlock ( epicsMutexId pSem ) 
{
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
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexLock ( epicsMutexId pSem ) 
{
    DWORD thisThread = GetCurrentThreadId ();

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
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexLockWithTimeout ( epicsMutexId pSem, double timeOut )
{ 
    static const unsigned mSecPerSec = 1000u;
    DWORD thisThread = GetCurrentThreadId ();

    EnterCriticalSection ( &pSem->cs );

    while ( pSem->count && pSem->threadId != thisThread ) {
        DWORD tmo;
        DWORD status;

        LeaveCriticalSection ( &pSem->cs );

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
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexTryLock ( epicsMutexId pSem ) 
{ 
    DWORD thisThread = GetCurrentThreadId ();

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
epicsShareFunc void epicsShareAPI epicsMutexShow ( epicsMutexId pSem, unsigned level ) 
{ 
    printf ("epicsMutex: count=%u, threadid=%x %s\n",
        pSem->count, pSem->threadId,
        pSem->threadId==GetCurrentThreadId()?
            "owned by this thread":"" );
}

#endif

