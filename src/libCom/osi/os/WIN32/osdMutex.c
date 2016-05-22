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
 *      WIN32 version
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 */

#include <stdio.h>
#include <limits.h>

#define VC_EXTRALEAN
#define STRICT
/* 
 * Defining this allows the *much* faster critical
 * section mutex primitive to be used. Unfortunately,
 * using certain of these functions drops support for W95\W98\WME 
 * unless we specify "delay loading" when we link with the 
 * DLL so that DLL entry points are not resolved until they 
 * are used. The code does have run time switches so
 * that the more advanced calls are not called unless 
 * they are available in the windows OS, but this feature
 * isnt going to be very useful unless we specify "delay 
 * loading" when we link with the DLL.
 *
 * It appears that the only entry point used here that causes
 * portability problems with W95\W98\WME is TryEnterCriticalSection.
 */
#ifndef _WIN32_WINNT
#   define _WIN32_WINNT 0x0400 
#endif
#include <windows.h>

#define epicsExportSharedSymbols
#include "shareLib.h"
#include "epicsMutex.h"
#include "epicsAssert.h"
#include "epicsStdio.h"

typedef struct epicsMutexOSD { 
    union {
        HANDLE mutex;
        CRITICAL_SECTION criticalSection;
    } os;
} epicsMutexOSD;

static BOOL thisIsNT = FALSE;
static LONG weHaveInitialized = 0;

/*
 * epicsMutexCreate ()
 */
epicsMutexOSD * epicsMutexOsdCreate ( void ) 
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
            InitializeCriticalSection ( &pSem->os.criticalSection );
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
void epicsMutexOsdDestroy ( epicsMutexOSD * pSem ) 
{    
    if ( thisIsNT ) {
        DeleteCriticalSection  ( &pSem->os.criticalSection );
    }
    else {
        CloseHandle ( pSem->os.mutex );
    }
    free ( pSem );
}

/*
 * epicsMutexOsdUnlock ()
 */
void epicsMutexOsdUnlock ( epicsMutexOSD * pSem ) 
{
    if ( thisIsNT ) {
        LeaveCriticalSection ( &pSem->os.criticalSection );
    }
    else {
        BOOL success = ReleaseMutex ( pSem->os.mutex );
        assert ( success );
    }
}

/*
 * epicsMutexOsdLock ()
 */
epicsMutexLockStatus epicsMutexOsdLock ( epicsMutexOSD * pSem ) 
{
    if ( thisIsNT ) {
        EnterCriticalSection ( &pSem->os.criticalSection );
    }
    else {
        DWORD status = WaitForSingleObject ( pSem->os.mutex, INFINITE );
        if ( status != WAIT_OBJECT_0 ) {
            return epicsMutexLockError;
        }
    }
    return epicsMutexLockOK;
}

/*
 * epicsMutexOsdTryLock ()
 */
epicsMutexLockStatus epicsMutexOsdTryLock ( epicsMutexOSD * pSem ) 
{ 
    if ( thisIsNT ) {
        if ( TryEnterCriticalSection ( &pSem->os.criticalSection ) ) {
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
 * epicsMutexOsdShow ()
 */
void epicsMutexOsdShow ( epicsMutexOSD * pSem, unsigned level ) 
{ 
    if ( thisIsNT ) {
        printf ("epicsMutex: win32 critical section at %p\n",
            (void * ) & pSem->os.criticalSection );
    }
    else {
        printf ( "epicsMutex: win32 mutex at %p\n", 
            ( void * ) pSem->os.mutex );
    }
}

