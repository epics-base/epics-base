/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* osdMutex.c */
/*
 *      $Id$
 *      WIN32 version
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
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

typedef struct epicsWin32CS { 
    CRITICAL_SECTION mutex;
    HANDLE unlockSignal;
    DWORD threadId;
    unsigned recursionCount;
    LONG waitingCount;
} epicsWin32CS;

typedef struct epicsMutexOSD { 
    union {
        HANDLE mutex;
        epicsWin32CS cs;
    } os;
} epicsMutexOSD;

static char thisIsNT = 0;
static LONG weHaveInitialized = 0;

/*
 * epicsMutexCreate ()
 */
epicsShareFunc epicsMutexId epicsShareAPI 
    epicsMutexOsdCreate ( void ) 
{
    epicsMutexOSD * pSem;

    if ( ! weHaveInitialized ) {
        BOOL status;
        OSVERSIONINFO osInfo;
        osInfo.dwOSVersionInfoSize = sizeof ( OSVERSIONINFO );
        status = GetVersionEx ( & osInfo );
        thisIsNT = status && ( osInfo.dwPlatformId == VER_PLATFORM_WIN32_NT );
        weHaveInitialized = 1;
    }
 
    pSem = malloc ( sizeof (*pSem) );
    if ( pSem ) {
        if ( thisIsNT ) {
            pSem->os.cs.unlockSignal = CreateEvent ( NULL, FALSE, FALSE, NULL );
            if ( pSem->os.cs.unlockSignal == 0 ) {
                free ( pSem );
                pSem = 0;
            }
            else {
                InitializeCriticalSection ( &pSem->os.cs.mutex );
                pSem->os.cs.threadId = 0;
                pSem->os.cs.recursionCount = 0u;
                pSem->os.cs.waitingCount = 0u;
            }
        }
        else {
            pSem->os.mutex = CreateMutex ( NULL, FALSE, NULL );
            if ( pSem->os.mutex == 0 ) {
                free ( pSem );
                pSem = 0;
            }
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
    if ( thisIsNT ) {
        assert ( pSem->os.cs.waitingCount == 0 );
        DeleteCriticalSection  ( &pSem->os.cs.mutex );
        CloseHandle ( pSem->os.cs.unlockSignal );
    }
    else {
        CloseHandle ( pSem->os.mutex );
    }
    free ( pSem );
}

/*
 * epicsMutexUnlock ()
 */
epicsShareFunc void epicsShareAPI 
    epicsMutexUnlock ( epicsMutexId pSem ) 
{
    if ( thisIsNT ) {
        //assert ( pSem->threadId == GetCurrentThreadId () );
        if ( pSem->os.cs.recursionCount == 1 ) {
            pSem->os.cs.threadId = 0;
            pSem->os.cs.recursionCount = 0;
            LeaveCriticalSection ( &pSem->os.cs.mutex );
            if ( pSem->os.cs.waitingCount ) {
                DWORD status = SetEvent ( pSem->os.cs.unlockSignal );
                assert ( status ); 
            }
        }
        else {
            assert ( pSem->os.cs.recursionCount > 1u );
            pSem->os.cs.recursionCount--;
        }
    }
    else {
        BOOL success = ReleaseMutex ( pSem->os.mutex );
        assert ( success );
    }
}

/*
 * epicsMutexLock ()
 */
epicsShareFunc epicsMutexLockStatus epicsShareAPI 
    epicsMutexLock ( epicsMutexId pSem ) 
{
    if ( thisIsNT ) {
        DWORD thisThread = GetCurrentThreadId ();
        if ( pSem->os.cs.threadId == thisThread ) {
            assert ( pSem->os.cs.recursionCount < UINT_MAX );
            pSem->os.cs.recursionCount++;
        }
        else {
            EnterCriticalSection ( &pSem->os.cs.mutex );
            pSem->os.cs.threadId = thisThread;
            pSem->os.cs.recursionCount = 1u;
        }
    }
    else {
        DWORD status = WaitForSingleObject ( pSem->os.mutex, INFINITE);
        if ( status != WAIT_OBJECT_0 ) {
            return epicsMutexLockError;
        }
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
    
    if ( thisIsNT ) {
        DWORD thisThread = GetCurrentThreadId ();
        DWORD begin = GetTickCount ();

        if ( pSem->os.cs.threadId == thisThread ) {
            assert ( pSem->os.cs.recursionCount < UINT_MAX );
            pSem->os.cs.recursionCount++;
            return epicsMutexLockOK;
        }
        else if ( ! TryEnterCriticalSection ( &pSem->os.cs.mutex ) ) {
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

            assert ( pSem->os.cs.waitingCount < 0x7FFFFFFF );
            InterlockedIncrement ( &pSem->os.cs.waitingCount );

            while ( ! TryEnterCriticalSection ( &pSem->os.cs.mutex ) ) {
                DWORD current;

                WaitForSingleObject ( pSem->os.cs.unlockSignal, tmo - delay );

                current = GetTickCount ();
                if ( current >= begin ) {
                    delay = current - begin;
                }
                else {
                    delay = ( 0xffffffff - begin ) + current + 1;
                }

                if ( delay >= tmo ) {
                    assert ( pSem->os.cs.waitingCount > 0 );
                    InterlockedDecrement ( &pSem->os.cs.waitingCount );
                    return epicsMutexLockTimeout;
                }
            }

            assert ( pSem->os.cs.waitingCount > 0 );
            InterlockedDecrement ( &pSem->os.cs.waitingCount );
        }

        pSem->os.cs.threadId = thisThread;
        pSem->os.cs.recursionCount = 1;
    }
    else {
        DWORD tmo;
        DWORD status;
        if ( timeOut <= 0.0 ) {
            tmo = 1u;
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
        status = WaitForSingleObject ( pSem->os.mutex, tmo );
        if ( status != WAIT_OBJECT_0 ) {
            if ( status == WAIT_TIMEOUT ) {
                return epicsMutexLockTimeout;
            }
            else {
                return epicsMutexLockError;
            }
        }
    }
    return epicsMutexLockOK;
}

/*
 * epicsMutexTryLock ()
 */
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexTryLock ( epicsMutexId pSem ) 
{ 
    if ( thisIsNT ) {
        DWORD thisThread = GetCurrentThreadId ();
        if ( pSem->os.cs.threadId == thisThread ) {
            assert ( pSem->os.cs.recursionCount < UINT_MAX );
            pSem->os.cs.recursionCount++;
        }
        else if ( TryEnterCriticalSection ( &pSem->os.cs.mutex ) ) {
            pSem->os.cs.threadId = thisThread;
            pSem->os.cs.recursionCount = 1;
            return epicsMutexLockOK;
        }
        else {
            return epicsMutexLockTimeout;
        }
    }
    else {
        DWORD status = WaitForSingleObject ( pSem->os.mutex, 0 );
        if ( status != WAIT_OBJECT_0 ) {
            if (status == WAIT_TIMEOUT) {
                return epicsMutexLockTimeout;
            }
            else {
                return epicsMutexLockError;
            }
        }
    }
    return epicsMutexLockOK;
}

/*
 * epicsMutexShow ()
 */
epicsShareFunc void epicsShareAPI epicsMutexShow ( epicsMutexId pSem, unsigned level ) 
{ 
    if ( thisIsNT ) {
        printf ("epicsMutex: recursion=%u, waiting=%d, threadid=%d %s\n",
            pSem->os.cs.recursionCount, pSem->os.cs.waitingCount, pSem->os.cs.threadId,
            pSem->os.cs.threadId==GetCurrentThreadId()?
                "owned by this thread":"" );
    }
    else {
        printf ( "epicsMutex: win32 mutex at %p\n", 
            ( void * ) pSem->os.mutex );
    }
}


