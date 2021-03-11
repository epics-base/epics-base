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
 */
#include <windows.h>

#define epicsExportSharedSymbols
#include "shareLib.h"
#include "epicsMutex.h"
#include "epicsAssert.h"
#include "epicsStdio.h"

typedef struct epicsMutexOSD { 
    union {
        CRITICAL_SECTION criticalSection;
    } os;
} epicsMutexOSD;

/*
 * epicsMutexCreate ()
 */
epicsMutexOSD * epicsMutexOsdCreate ( void ) 
{
    epicsMutexOSD * pSem;
 
    pSem = malloc ( sizeof (*pSem) );
    if ( pSem ) {
        InitializeCriticalSection ( &pSem->os.criticalSection );
    }    
    return pSem;
}

/*
 * epicsMutexOsdDestroy ()
 */
void epicsMutexOsdDestroy ( epicsMutexOSD * pSem ) 
{    
    DeleteCriticalSection  ( &pSem->os.criticalSection );
    free ( pSem );
}

/*
 * epicsMutexOsdUnlock ()
 */
void epicsMutexOsdUnlock ( epicsMutexOSD * pSem ) 
{
    LeaveCriticalSection ( &pSem->os.criticalSection );
}

/*
 * epicsMutexOsdLock ()
 */
epicsMutexLockStatus epicsMutexOsdLock ( epicsMutexOSD * pSem ) 
{
    EnterCriticalSection ( &pSem->os.criticalSection );
    return epicsMutexLockOK;
}

/*
 * epicsMutexOsdTryLock ()
 */
epicsMutexLockStatus epicsMutexOsdTryLock ( epicsMutexOSD * pSem ) 
{ 
    if ( TryEnterCriticalSection ( &pSem->os.criticalSection ) ) {
        return epicsMutexLockOK;
    }
    else {
        return epicsMutexLockTimeout;
    }
}

/*
 * epicsMutexOsdShow ()
 */
void epicsMutexOsdShow ( epicsMutexOSD * pSem, unsigned level ) 
{ 
    printf ("epicsMutex: win32 critical section at %p\n",
            (void * ) & pSem->os.criticalSection );
}

