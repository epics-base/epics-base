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

typedef struct epicsMutexOSD { 
    CRITICAL_SECTION mutex;
    HANDLE unlockSignal;
    DWORD threadId;
    unsigned recursionCount;
    LONG waitingCount;
} epicsMutexOSD;

/*
 * epicsMutexCreate ()
 */
epicsShareFunc epicsMutexId epicsShareAPI 
    epicsMutexOsdCreate ( void ) 
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
            InitializeCriticalSection ( &pSem->mutex );
            pSem->threadId = 0;
            pSem->recursionCount = 0u;
            pSem->waitingCount = 0u;
        }
    }    
    return pSem;
}

/*
 * epicsMutexOsdDestroy ()
 */
epicsShareFunc void epicsShareAPI 
    epicsMutexOsdDestroy ( epicsMutexId pSem ) 
{    
    assert ( pSem->waitingCount == 0 );
    DeleteCriticalSection  ( &pSem->mutex );
    CloseHandle ( pSem->unlockSignal );
    free ( pSem );
}

/*
 * epicsMutexUnlock ()
 */
epicsShareFunc void epicsShareAPI 
    epicsMutexUnlock ( epicsMutexId pSem ) 
{
    //assert ( pSem->threadId == GetCurrentThreadId () );
    if ( pSem->recursionCount == 1 ) {
        pSem->threadId = 0;
        pSem->recursionCount = 0;
        LeaveCriticalSection ( &pSem->mutex );
        if ( pSem->waitingCount ) {
            DWORD status = SetEvent ( pSem->unlockSignal );
            assert ( status ); 
        }
    }
    else {
        assert ( pSem->recursionCount > 1u );
        pSem->recursionCount--;
    }
}

/*
 * epicsMutexLock ()
 */
epicsShareFunc epicsMutexLockStatus epicsShareAPI 
    epicsMutexLock ( epicsMutexId pSem ) 
{
    DWORD thisThread = GetCurrentThreadId ();

    if ( pSem->threadId == thisThread ) {
        assert ( pSem->recursionCount < UINT_MAX );
        pSem->recursionCount++;
    }
    else {
        EnterCriticalSection ( &pSem->mutex );
        pSem->threadId = thisThread;
        pSem->recursionCount = 1u;
    }

    return epicsMutexLockOK;
}

/*
 * epicsMutexLockWithTimeout ()
 */
epicsShareFunc epicsMutexLockStatus epicsShareAPI 
    epicsMutexLockWithTimeout ( epicsMutexId pSem, double timeOut )
{ 
    static const unsigned mSecPerSec = 1000u;
    DWORD thisThread = GetCurrentThreadId ();
    DWORD begin = GetTickCount ();

    if ( pSem->threadId == thisThread ) {
        assert ( pSem->recursionCount < UINT_MAX );
        pSem->recursionCount++;
        return epicsMutexLockOK;
    }
    else if ( ! TryEnterCriticalSection ( &pSem->mutex ) ) {
        DWORD delay = 0;
        DWORD tmo;

        if ( timeOut <= 0.0 ) {
            tmo = 1;
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

        assert ( pSem->waitingCount < 0x7FFFFFFF );
        InterlockedIncrement ( &pSem->waitingCount );

        while ( ! TryEnterCriticalSection ( &pSem->mutex ) ) {
            DWORD current;

            WaitForSingleObject ( pSem->unlockSignal, tmo - delay );

            current = GetTickCount ();
            if ( current >= begin ) {
                delay = current - begin;
            }
            else {
                delay = ( 0xffffffff - begin ) + current + 1;
            }

            if ( delay >= tmo ) {
                assert ( pSem->waitingCount > 0 );
                InterlockedDecrement ( &pSem->waitingCount );
                return epicsMutexLockTimeout;
            }
        }

        assert ( pSem->waitingCount > 0 );
        InterlockedDecrement ( &pSem->waitingCount );
    }

    pSem->threadId = thisThread;
    pSem->recursionCount = 1;

    return epicsMutexLockOK;
}

/*
 * epicsMutexTryLock ()
 */
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexTryLock ( epicsMutexId pSem ) 
{ 
    DWORD thisThread = GetCurrentThreadId ();

    if ( pSem->threadId == thisThread ) {
        assert ( pSem->recursionCount < UINT_MAX );
        pSem->recursionCount++;
        return epicsMutexLockOK;
    }
    else if ( TryEnterCriticalSection ( &pSem->mutex ) ) {
        pSem->threadId = thisThread;
        pSem->recursionCount = 1;
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
    printf ("epicsMutex: recursion=%u, waiting=%d, threadid=%d %s\n",
        pSem->recursionCount, pSem->waitingCount, pSem->threadId,
        pSem->threadId==GetCurrentThreadId()?
            "owned by this thread":"" );
}


