
/*
 * $Id$
 *
 * Author: Jeff Hill
 * 
 *
 */

#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#ifndef VC_EXTRALEAN
#   define VC_EXTRALEAN
#endif
#ifndef WIN32_LEAN_AND_MEAN 
#   define WIN32_LEAN_AND_MEAN 
#endif
#include <winsock2.h>
#include <process.h>

#define epicsExportSharedSymbols
#include "shareLib.h"
#include "epicsThread.h"
#include "cantProceed.h"
#include "errlog.h"
#include "epicsAssert.h"

typedef struct win32ThreadParam {
    HANDLE handle;
    EPICSTHREADFUNC funptr;
    void *parm;
    char *pName;
    DWORD id;
    char isSuspended;
} win32ThreadParam;

static win32ThreadParam anonymousThreadParm = 
    {0, 0, 0, "anonymous", 0, 0};

static DWORD tlsIndexWIN32 = 0xFFFFFFFF;

typedef struct osdThreadPrivate {
    DWORD key;
} osdThreadPrivate;


static HANDLE win32ThreadGlobalMutex = 0;
static int win32ThreadInitOK = 0;

#define osdPriorityStateCount 7u
static const int osdPriorityList [osdPriorityStateCount] = 
{
    THREAD_PRIORITY_IDLE,
    THREAD_PRIORITY_LOWEST,
    THREAD_PRIORITY_BELOW_NORMAL,
    THREAD_PRIORITY_NORMAL,
    THREAD_PRIORITY_ABOVE_NORMAL,
    THREAD_PRIORITY_HIGHEST,
    THREAD_PRIORITY_TIME_CRITICAL,
};

/*
 * threadCleanupWIN32 ()
 */
static void threadCleanupWIN32 (void)
{
    if (tlsIndexWIN32!=0xFFFFFFFF) {
        TlsFree (tlsIndexWIN32);
        tlsIndexWIN32 = 0xFFFFFFFF;
    }

    if (win32ThreadGlobalMutex) {
        CloseHandle (win32ThreadGlobalMutex);
        win32ThreadGlobalMutex = NULL;
    }
}

/*
 * epicsThreadExitMain ()
 */
epicsShareFunc void epicsShareAPI epicsThreadExitMain (void)
{
}

/*
 * epicsThreadInit ()
 */
epicsShareFunc void epicsShareAPI epicsThreadInit (void)
{
    HANDLE win32ThreadGlobalMutexTmp;
    DWORD status;
    BOOL success;
    int crtlStatus;

    if (win32ThreadGlobalMutex) {
        /* wait for init to complete */
        status = WaitForSingleObject (win32ThreadGlobalMutex, INFINITE);
        assert ( status == WAIT_OBJECT_0 );
        success = ReleaseMutex (win32ThreadGlobalMutex);
        assert (success);
        return;
    }
    else {
        win32ThreadGlobalMutexTmp = CreateMutex (NULL, TRUE, NULL);
        if ( win32ThreadGlobalMutexTmp == 0 ) {
            return;
        }

#if 1
        /* not arch neutral, but at least supported by w95 and borland */
        if ( InterlockedExchange ( (LPLONG) &win32ThreadGlobalMutex, (LONG) win32ThreadGlobalMutexTmp ) ) {
#else
        /* not supported on W95, but the alternative requires assuming that pointer and integer are the same */
        if (InterlockedCompareExchange ( (PVOID *) &win32ThreadGlobalMutex, (PVOID) win32ThreadGlobalMutexTmp, (PVOID)0 ) != 0) {
#endif
            CloseHandle (win32ThreadGlobalMutexTmp);
            /* wait for init to complete */
            status = WaitForSingleObject (win32ThreadGlobalMutex, INFINITE);
            assert ( status == WAIT_OBJECT_0 );
            success = ReleaseMutex (win32ThreadGlobalMutex);
            assert (success);
            return;
        }
    }

    if (tlsIndexWIN32==0xFFFFFFFF) {
        tlsIndexWIN32 = TlsAlloc();
        if (tlsIndexWIN32==0xFFFFFFFF) {
            success = ReleaseMutex (win32ThreadGlobalMutex);
            assert (success);
            return;
        }
    }

    crtlStatus = atexit (threadCleanupWIN32);
    if (crtlStatus) {
        success = ReleaseMutex (win32ThreadGlobalMutex);
        assert (success);
        return;
    }

    win32ThreadInitOK = TRUE;

    success = ReleaseMutex (win32ThreadGlobalMutex);
    assert (success);
}

/*
 * osdPriorityMagFromPriorityOSI ()
 */
static unsigned osdPriorityMagFromPriorityOSI ( unsigned osiPriority ) 
{
    unsigned magnitude;

    // optimizer will remove this one if epicsThreadPriorityMin is zero
    // and osiPriority is unsigned
    if (osiPriority<epicsThreadPriorityMin) {
        osiPriority = epicsThreadPriorityMin;
    }

    if (osiPriority>epicsThreadPriorityMax) {
        osiPriority = epicsThreadPriorityMax;
    }

    magnitude = osiPriority * osdPriorityStateCount;
    magnitude /= (epicsThreadPriorityMax - epicsThreadPriorityMin)+1;

    return magnitude;
}

/*
 * epicsThreadGetOsdPriorityValue ()
 */
static int epicsThreadGetOsdPriorityValue ( unsigned osiPriority ) 
{
    unsigned magnitude = osdPriorityMagFromPriorityOSI ( osiPriority );
    return osdPriorityList[magnitude];
}

/*
 * osdPriorityMagFromPriorityOSI ()
 */
static unsigned osdPriorityMagFromPriorityOSD ( int osdPriority ) 
{
    unsigned magnitude;

    for ( magnitude=0u; magnitude<osdPriorityStateCount; magnitude++ ) {
        if ( osdPriority == osdPriorityList[magnitude] ) {
            break;
        }
    }

    assert ( magnitude < osdPriorityStateCount );

    return magnitude;
}

/*
 * osdPriorityMagFromPriorityOSI ()
 */
static unsigned osiPriorityMagFromMagnitueOSD ( unsigned magnitude ) 
{
    unsigned osiPriority;

    osiPriority = magnitude * (epicsThreadPriorityMax - epicsThreadPriorityMin);
    osiPriority /= osdPriorityStateCount - 1u;
    osiPriority += epicsThreadPriorityMin;

    return osiPriority;
}


/* 
 * epicsThreadGetOsiPriorityValue ()
 */
static unsigned epicsThreadGetOsiPriorityValue ( int osdPriority ) 
{
    unsigned magnitude = osdPriorityMagFromPriorityOSD ( osdPriority );
    return osiPriorityMagFromMagnitueOSD (magnitude);
}

/*
 * epicsThreadLowestPriorityLevelAbove ()
 */
epicsShareFunc epicsThreadBooleanStatus epicsShareAPI epicsThreadLowestPriorityLevelAbove 
            (unsigned int priority, unsigned *pPriorityJustAbove)
{
    unsigned magnitude = osdPriorityMagFromPriorityOSI ( priority );
    epicsThreadBooleanStatus status;

    if ( magnitude < (osdPriorityStateCount-1) ) {
        *pPriorityJustAbove = osiPriorityMagFromMagnitueOSD ( magnitude + 1u );
        status = epicsThreadBooleanStatusSuccess;
    }
    else {
        status = epicsThreadBooleanStatusFail;
    }
    return status;
}

/*
 * epicsThreadHighestPriorityLevelBelow ()
 */
epicsShareFunc epicsThreadBooleanStatus epicsShareAPI epicsThreadHighestPriorityLevelBelow 
            (unsigned int priority, unsigned *pPriorityJustBelow)
{
    unsigned magnitude = osdPriorityMagFromPriorityOSI ( priority );
    epicsThreadBooleanStatus status;

    if ( magnitude > 1u ) {
        *pPriorityJustBelow = osiPriorityMagFromMagnitueOSD ( magnitude - 1u );
        status = epicsThreadBooleanStatusSuccess;
    }
    else {
        status = epicsThreadBooleanStatusFail;
    }
    return status;
}

/*
 * epicsThreadGetStackSize ()
 */
epicsShareFunc unsigned int epicsShareAPI epicsThreadGetStackSize (epicsThreadStackSizeClass stackSizeClass) 
{
    static const unsigned stackSizeTable[epicsThreadStackBig+1] = {4000, 6000, 11000};

    if (stackSizeClass<epicsThreadStackSmall) {
        errlogPrintf("epicsThreadGetStackSize illegal argument (too small)");
        return stackSizeTable[epicsThreadStackBig];
    }

    if (stackSizeClass>epicsThreadStackBig) {
        errlogPrintf("epicsThreadGetStackSize illegal argument (too large)");
        return stackSizeTable[epicsThreadStackBig];
    }

    return stackSizeTable[stackSizeClass];
}

/*
 * epicsWin32ThreadEntry()
 */
static unsigned WINAPI epicsWin32ThreadEntry (LPVOID lpParameter)
{
    win32ThreadParam *pParm = (win32ThreadParam *) lpParameter;
    BOOL stat;

    stat = TlsSetValue ( tlsIndexWIN32, pParm );
    if (stat) {
        ( *pParm->funptr ) ( pParm->parm );
        TlsSetValue ( tlsIndexWIN32, 0 );
    }

    /*
     * CAUTION: !!!! the thread id might continue to be used after this thread exits !!!!
     */
    free ( pParm );

    return ( (unsigned) stat ); /* this indirectly closes the thread handle */
}

/*
 * epicsThreadCreate ()
 */
epicsShareFunc epicsThreadId epicsShareAPI epicsThreadCreate (const char *pName,
    unsigned int priority, unsigned int stackSize, EPICSTHREADFUNC pFunc,void *pParm)
{
    win32ThreadParam *pParmWIN32;
    int osdPriority;
    DWORD wstat;
    BOOL bstat;

    if ( ! win32ThreadInitOK ) {
        epicsThreadInit ();
        if ( ! win32ThreadInitOK ) {
            return NULL;
        }
    }

    pParmWIN32 = malloc ( sizeof ( *pParmWIN32 ) + strlen ( pName ) + 1 );
    if ( pParmWIN32 == NULL ) {
        return NULL;
    }

    pParmWIN32->pName = (char *) ( pParmWIN32 + 1 );
    strcpy ( pParmWIN32->pName, pName );
    pParmWIN32->funptr = pFunc;
    pParmWIN32->parm = pParm;
    pParmWIN32->isSuspended = 0;

    pParmWIN32->handle = (HANDLE) _beginthreadex ( 0, stackSize, epicsWin32ThreadEntry, 
        pParmWIN32, CREATE_SUSPENDED, &pParmWIN32->id );
    if ( pParmWIN32->handle == 0 ) {
        free ( pParmWIN32 );
        return NULL;
    }

    osdPriority = epicsThreadGetOsdPriorityValue (priority);
    bstat = SetThreadPriority ( pParmWIN32->handle, osdPriority );
    if (!bstat) {
        CloseHandle ( pParmWIN32->handle ); 
        free ( pParmWIN32 );
        return NULL;
    }

    wstat =  ResumeThread ( pParmWIN32->handle );
    if (wstat==0xFFFFFFFF) {
        CloseHandle ( pParmWIN32->handle ); 
        free ( pParmWIN32 );
        return NULL;
    }

    return ( epicsThreadId ) pParmWIN32;
}

/*
 * epicsThreadSuspendSelf ()
 */
epicsShareFunc void epicsShareAPI epicsThreadSuspendSelf ()
{
    win32ThreadParam *pParm = (win32ThreadParam *) TlsGetValue (tlsIndexWIN32);
    BOOL success;
    DWORD stat;

    if (pParm) {
        stat = WaitForSingleObject (win32ThreadGlobalMutex, INFINITE);
        assert ( stat == WAIT_OBJECT_0 );

        stat =  SuspendThread ( pParm->handle );
        pParm->isSuspended = 1;

        success = ReleaseMutex (win32ThreadGlobalMutex);
        assert (success);
    }
    else {
        stat =  SuspendThread ( GetCurrentThread () );
    }
    assert (stat!=0xFFFFFFFF);
}

/*
 * epicsThreadResume ()
 */
epicsShareFunc void epicsShareAPI epicsThreadResume (epicsThreadId id)
{
    win32ThreadParam *pParm = (win32ThreadParam *) id;
    BOOL success;
    DWORD stat;

    stat = WaitForSingleObject (win32ThreadGlobalMutex, INFINITE);
    assert ( stat == WAIT_OBJECT_0 );

    stat =  ResumeThread ( pParm->handle );
    pParm->isSuspended = 0;

    success = ReleaseMutex (win32ThreadGlobalMutex);
    assert (success);

    assert (stat!=0xFFFFFFFF);
}

/*
 * epicsThreadGetPriority ()
 */
epicsShareFunc unsigned epicsShareAPI epicsThreadGetPriority (epicsThreadId id) 
{ 
    win32ThreadParam *pParm = (win32ThreadParam *) id;
    int win32ThreadPriority;

    win32ThreadPriority = GetThreadPriority (pParm->handle);
    assert (win32ThreadPriority!=THREAD_PRIORITY_ERROR_RETURN);
 
    return epicsThreadGetOsiPriorityValue (win32ThreadPriority);
}

/*
 * epicsThreadGetPriority ()
 */
epicsShareFunc unsigned epicsShareAPI epicsThreadGetPrioritySelf () 
{ 
    int win32ThreadPriority;

    win32ThreadPriority = GetThreadPriority ( GetCurrentThread () );
    assert (win32ThreadPriority!=THREAD_PRIORITY_ERROR_RETURN);
 
    return epicsThreadGetOsiPriorityValue (win32ThreadPriority);
}

/*
 * epicsThreadSetPriority ()
 */
epicsShareFunc void epicsShareAPI epicsThreadSetPriority (epicsThreadId id, unsigned priority) 
{
    win32ThreadParam *pParm = (win32ThreadParam *) id;
    BOOL stat;

    stat = SetThreadPriority (pParm->handle, epicsThreadGetOsdPriorityValue (priority) );
    assert (stat);
}

/*
 * epicsThreadIsEqual ()
 */
epicsShareFunc int epicsShareAPI epicsThreadIsEqual (epicsThreadId id1, epicsThreadId id2) 
{
    return ( id1 == id2 );
}

/*
 * epicsThreadIsSuspended ()
 *
 * This implementation is deficient if the thread is not suspended by
 * epicsThreadSuspendSelf () or resumed by epicsThreadResume(). This would happen
 * if a WIN32 call was used instead of epicsThreadSuspendSelf(), or if WIN32
 * suspended the thread when it receives an unhandled run time exception.
 * 
 */
epicsShareFunc int epicsShareAPI epicsThreadIsSuspended (epicsThreadId id) 
{
    win32ThreadParam *pParm = (win32ThreadParam *) id;
    DWORD exitCode;
    BOOL stat;
    
    stat = GetExitCodeThread (pParm->handle, &exitCode);
    if (stat) {
        if (exitCode!=STILL_ACTIVE) {
            return 1;
        }
        else {
            return pParm->isSuspended;
        }
    }
    else {
        return 1;
    }
}

/*
 * epicsThreadSleep ()
 */
epicsShareFunc void epicsShareAPI epicsThreadSleep (double seconds)
{
    static const double mSecPerSec = 1000;
    DWORD milliSecDelay = (DWORD) (seconds * mSecPerSec);
    Sleep (milliSecDelay);
}

/*
 * epicsThreadGetIdSelf ()
 */
epicsShareFunc epicsThreadId epicsShareAPI epicsThreadGetIdSelf (void) 
{
    win32ThreadParam *pParm = (win32ThreadParam *) TlsGetValue (tlsIndexWIN32);
    return (epicsThreadId) pParm;
}

epicsShareFunc epicsThreadId epicsShareAPI epicsThreadGetId (const char *name)
{
    return 0;
}


/*
 * epicsThreadGetNameSelf ()
 */
epicsShareFunc const char * epicsShareAPI epicsThreadGetNameSelf (void)
{
    win32ThreadParam *pParm = (win32ThreadParam *) TlsGetValue (tlsIndexWIN32);

    if (pParm) {
        return pParm->pName;
    }
    else {
        return "anonymous";
    }
}

/*
 * epicsThreadGetName ()
 */
epicsShareFunc void epicsShareAPI epicsThreadGetName (epicsThreadId id, char *pName, size_t size)
{
    win32ThreadParam *pParm = (win32ThreadParam *) id;

    if (size) {
        size_t sizeMinusOne = size-1;
        strncpy (pName, pParm->pName, sizeMinusOne);
        pName [sizeMinusOne] = '\0';
    }
}

/*
 * epicsThreadShowAll ()
 */
epicsShareFunc void epicsShareAPI epicsThreadShowAll (unsigned level)
{
    printf ("sorry, epicsThreadShowAll() not implemented on WIN32\n");
}

/*
 * epicsThreadShow ()
 */
epicsShareFunc void epicsShareAPI epicsThreadShow (epicsThreadId id, unsigned level)
{
    win32ThreadParam *pParm = (win32ThreadParam *) id;

    printf ("thread \"%s\" HANDLE=%p func=%p parm=%p %s %d\n",
        pParm->pName, pParm->handle, pParm->funptr, pParm->parm,
        pParm->isSuspended?"suspended":"running", pParm->id);
}

/*
 * epicsThreadOnce ()
 */
epicsShareFunc void epicsShareAPI epicsThreadOnceOsd (
    epicsThreadOnceId *id, void (*func)(void *), void *arg)
{

    win32ThreadParam *pParm = (win32ThreadParam *) id;
    BOOL success;
    DWORD stat;

    if ( ! win32ThreadInitOK ) {
        epicsThreadInit ();
        if ( ! win32ThreadInitOK ) {
            return;
        }
    }

    stat = WaitForSingleObject ( win32ThreadGlobalMutex, INFINITE );
    assert ( stat == WAIT_OBJECT_0 );

    if (!*id) {
        *id = 1;
        (*func) (arg);
    }

    success = ReleaseMutex (win32ThreadGlobalMutex);
    assert (success);
}

/*
 * epicsThreadPrivateCreate ()
 */
epicsShareFunc epicsThreadPrivateId epicsShareAPI epicsThreadPrivateCreate ()
{
    osdThreadPrivate *p = (osdThreadPrivate *) malloc (sizeof (*p));
    if (p) {
        p->key = TlsAlloc ();
        if (p->key==0xFFFFFFFF) {
            free (p);
            p = 0;
        }
    }
    return (epicsThreadPrivateId) p;
}

/*
 * epicsThreadPrivateDelete ()
 */
epicsShareFunc void epicsShareAPI epicsThreadPrivateDelete (epicsThreadPrivateId id)
{
    osdThreadPrivate *p = (osdThreadPrivate *) id;
    BOOL stat = TlsFree (p->key);
    assert (stat);
    free (p);
}

/*
 * epicsThreadPrivateSet ()
 */
epicsShareFunc void epicsShareAPI epicsThreadPrivateSet (epicsThreadPrivateId id, void *pVal)
{
    struct osdThreadPrivate *pPvt = (struct osdThreadPrivate *) id;
    BOOL stat = TlsSetValue (pPvt->key, (void *) pVal );
    assert (stat);
}

/*
 * epicsThreadPrivateGet ()
 */
epicsShareFunc void * epicsShareAPI epicsThreadPrivateGet (epicsThreadPrivateId id)
{
    struct osdThreadPrivate *pPvt = (struct osdThreadPrivate *) id;
    return (void *) TlsGetValue (pPvt->key);
}

#ifdef TEST_CODES
void testPriorityMapping ()
{
    unsigned i;

    for (i=epicsThreadPriorityMin; i<epicsThreadPriorityMax; i++) {
        printf ("%u %d\n", i, epicsThreadGetOsdPriorityValue (i) );
    }

    for (i=0; i<osdPriorityStateCount; i++) {
        printf ("%d %u\n", osdPriorityList[i], epicsThreadGetOsiPriorityValue(osdPriorityList[i]));
    }
    return 0;
}
#endif
