
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
#include "osiThread.h"
#include "cantProceed.h"
#include "errlog.h"
#include "epicsAssert.h"

typedef struct {
    HANDLE handle;
    THREADFUNC funptr;
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
 * threadInit ()
 */
epicsShareFunc void epicsShareAPI threadInit (void)
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

        /* not supported on W95, but the alternative requires assuming that pointer and integer are the same */
        if (InterlockedCompareExchange ( (PVOID *) &win32ThreadGlobalMutex, (PVOID) win32ThreadGlobalMutexTmp, (PVOID)0 ) != 0) {
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

    // optimizer will remove this one if threadPriorityMin is zero
    // and osiPriority is unsigned
    if (osiPriority<threadPriorityMin) {
        osiPriority = threadPriorityMin;
    }

    if (osiPriority>threadPriorityMax) {
        osiPriority = threadPriorityMax;
    }

    magnitude = osiPriority * osdPriorityStateCount;
    magnitude /= (threadPriorityMax - threadPriorityMin)+1;

    return magnitude;
}

/*
 * threadGetOsdPriorityValue ()
 */
static int threadGetOsdPriorityValue ( unsigned osiPriority ) 
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

    osiPriority = magnitude * (threadPriorityMax - threadPriorityMin);
    osiPriority /= osdPriorityStateCount - 1u;
    osiPriority += threadPriorityMin;

    return osiPriority;
}


/* 
 * threadGetOsiPriorityValue ()
 */
static unsigned threadGetOsiPriorityValue ( int osdPriority ) 
{
    unsigned magnitude = osdPriorityMagFromPriorityOSD ( osdPriority );
    return osiPriorityMagFromMagnitueOSD (magnitude);
}

/*
 * threadLowestPriorityLevelAbove ()
 */
epicsShareFunc threadBoolStatus epicsShareAPI threadLowestPriorityLevelAbove 
            (unsigned int priority, unsigned *pPriorityJustAbove)
{
    unsigned magnitude = osdPriorityMagFromPriorityOSI ( priority );
    threadBoolStatus status;

    if ( magnitude < (osdPriorityStateCount-1) ) {
        *pPriorityJustAbove = osiPriorityMagFromMagnitueOSD ( magnitude + 1u );
        status = tbsSuccess;
    }
    else {
        status = tbsFail;
    }
    return status;
}

/*
 * threadHighestPriorityLevelBelow ()
 */
epicsShareFunc threadBoolStatus epicsShareAPI threadHighestPriorityLevelBelow 
            (unsigned int priority, unsigned *pPriorityJustBelow)
{
    unsigned magnitude = osdPriorityMagFromPriorityOSI ( priority );
    threadBoolStatus status;

    if ( magnitude > 1u ) {
        *pPriorityJustBelow = osiPriorityMagFromMagnitueOSD ( magnitude - 1u );
        status = tbsSuccess;
    }
    else {
        status = tbsFail;
    }
    return status;
}

/*
 * threadGetStackSize ()
 */
epicsShareFunc unsigned int epicsShareAPI threadGetStackSize (threadStackSizeClass stackSizeClass) 
{
    static const unsigned stackSizeTable[threadStackBig+1] = {4000, 6000, 11000};

    if (stackSizeClass<threadStackSmall) {
        errlogPrintf("threadGetStackSize illegal argument (too small)");
        return stackSizeTable[threadStackBig];
    }

    if (stackSizeClass>threadStackBig) {
        errlogPrintf("threadGetStackSize illegal argument (too large)");
        return stackSizeTable[threadStackBig];
    }

    return stackSizeTable[stackSizeClass];
}

/*
 * epicsWin32ThreadEntry()
 */
static int WINAPI epicsWin32ThreadEntry (LPVOID lpParameter)
{
    win32ThreadParam *pParm = (win32ThreadParam *) lpParameter;
    BOOL stat;

    stat = TlsSetValue (tlsIndexWIN32, pParm);
    if (stat) {
        (*pParm->funptr) (pParm->parm);
        TlsSetValue (tlsIndexWIN32, 0);
    }

    /*
     * CAUTION: !!!! the thread id might continue to be used after this thread exits !!!!
     */
    free (pParm);
    return (stat); /* this indirectly closes the thread handle */
}

/*
 * threadCreate ()
 */
epicsShareFunc threadId epicsShareAPI threadCreate (const char *pName,
    unsigned int priority, unsigned int stackSize, THREADFUNC pFunc,void *pParm)
{
    win32ThreadParam *pParmWIN32;
    int osdPriority;
    DWORD wstat;
    BOOL bstat;

    if (!win32ThreadInitOK) {
        threadInit ();
        if (!win32ThreadInitOK) {
            return NULL;
        }
    }

    pParmWIN32 = malloc ( sizeof (*pParmWIN32) + strlen (pName) + 1 );
    if ( pParmWIN32 == NULL ) {
        return NULL;
    }

    pParmWIN32->pName = (char *) (pParmWIN32 + 1);
    strcpy (pParmWIN32->pName, pName);
    pParmWIN32->funptr = pFunc;
    pParmWIN32->parm = pParm;
    pParmWIN32->isSuspended = 0;

    pParmWIN32->handle = (HANDLE) _beginthreadex (0, stackSize, epicsWin32ThreadEntry, 
        pParmWIN32, CREATE_SUSPENDED, &pParmWIN32->id);
    if ( pParmWIN32->handle == 0 ) {
        free (pParmWIN32);
        return NULL;
    }

    osdPriority = threadGetOsdPriorityValue (priority);
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

    return (threadId) pParmWIN32;
}

/*
 * threadSuspendSelf ()
 */
epicsShareFunc void epicsShareAPI threadSuspendSelf ()
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
 * threadResume ()
 */
epicsShareFunc void epicsShareAPI threadResume (threadId id)
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
 * threadGetPriority ()
 */
epicsShareFunc unsigned epicsShareAPI threadGetPriority (threadId id) 
{ 
    win32ThreadParam *pParm = (win32ThreadParam *) id;
    int win32ThreadPriority;

    win32ThreadPriority = GetThreadPriority (pParm->handle);
    assert (win32ThreadPriority!=THREAD_PRIORITY_ERROR_RETURN);
 
    return threadGetOsiPriorityValue (win32ThreadPriority);
}

/*
 * threadGetPriority ()
 */
epicsShareFunc unsigned epicsShareAPI threadGetPrioritySelf () 
{ 
    int win32ThreadPriority;

    win32ThreadPriority = GetThreadPriority ( GetCurrentThread () );
    assert (win32ThreadPriority!=THREAD_PRIORITY_ERROR_RETURN);
 
    return threadGetOsiPriorityValue (win32ThreadPriority);
}

/*
 * threadSetPriority ()
 */
epicsShareFunc void epicsShareAPI threadSetPriority (threadId id, unsigned priority) 
{
    win32ThreadParam *pParm = (win32ThreadParam *) id;
    BOOL stat;

    stat = SetThreadPriority (pParm->handle, threadGetOsdPriorityValue (priority) );
    assert (stat);
}

/*
 * threadIsEqual ()
 */
epicsShareFunc int epicsShareAPI threadIsEqual (threadId id1, threadId id2) 
{
    return ( id1 == id2 );
}

/*
 * threadIsSuspended ()
 *
 * This implementation is deficient if the thread is not suspended by
 * threadSuspendSelf () or resumed by threadResume(). This would happen
 * if a WIN32 call was used instead of threadSuspendSelf(), or if WIN32
 * suspended the thread when it receives an unhandled run time exception.
 * 
 */
epicsShareFunc int epicsShareAPI threadIsSuspended (threadId id) 
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
 * threadSleep ()
 */
epicsShareFunc void epicsShareAPI threadSleep (double seconds)
{
    static const double mSecPerSec = 1000;
    DWORD milliSecDelay = (DWORD) (seconds * mSecPerSec);
    Sleep (milliSecDelay);
}

/*
 * threadGetIdSelf ()
 */
epicsShareFunc threadId epicsShareAPI threadGetIdSelf (void) 
{
    win32ThreadParam *pParm = (win32ThreadParam *) TlsGetValue (tlsIndexWIN32);
    return (threadId) pParm;
}

/*
 * threadGetNameSelf ()
 */
epicsShareFunc const char * epicsShareAPI threadGetNameSelf (void)
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
 * threadGetName ()
 */
epicsShareFunc void epicsShareAPI threadGetName (threadId id, char *pName, size_t size)
{
    win32ThreadParam *pParm = (win32ThreadParam *) id;

    if (size) {
        size_t sizeMinusOne = size-1;
        strncpy (pName, pParm->pName, sizeMinusOne);
        pName [sizeMinusOne] = '\0';
    }
}

/*
 * threadShowAll ()
 */
epicsShareFunc void epicsShareAPI threadShowAll (unsigned level)
{
    printf ("sorry, threadShowAll() not implemented on WIN32\n");
}

/*
 * threadShow ()
 */
epicsShareFunc void epicsShareAPI threadShow (threadId id, unsigned level)
{
    win32ThreadParam *pParm = (win32ThreadParam *) id;

    printf ("thread \"%s\" HANDLE=%p func=%p parm=%p %s %d\n",
        pParm->pName, pParm->handle, pParm->funptr, pParm->parm,
        pParm->isSuspended?"suspended":"running", pParm->id);
}

/*
 * threadOnce ()
 */
epicsShareFunc void epicsShareAPI threadOnceOsd (
    threadOnceId *id, void (*func)(void *), void *arg)
{

    win32ThreadParam *pParm = (win32ThreadParam *) id;
    BOOL success;
    DWORD stat;

    stat = WaitForSingleObject (win32ThreadGlobalMutex, INFINITE);
    assert ( stat == WAIT_OBJECT_0 );

    if (!*id) {
        *id = 1;
        (*func) (arg);
    }

    success = ReleaseMutex (win32ThreadGlobalMutex);
    assert (success);
}

/*
 * threadPrivateCreate ()
 */
epicsShareFunc threadPrivateId epicsShareAPI threadPrivateCreate ()
{
    osdThreadPrivate *p = (osdThreadPrivate *) malloc (sizeof (*p));
    if (p) {
        p->key = TlsAlloc ();
        if (p->key==0xFFFFFFFF) {
            free (p);
            p = 0;
        }
    }
    return (threadPrivateId) p;
}

/*
 * threadPrivateDelete ()
 */
epicsShareFunc void epicsShareAPI threadPrivateDelete (threadPrivateId id)
{
    osdThreadPrivate *p = (osdThreadPrivate *) id;
    BOOL stat = TlsFree (p->key);
    assert (stat);
    free (p);
}

/*
 * threadPrivateSet ()
 */
epicsShareFunc void epicsShareAPI threadPrivateSet (threadPrivateId id, void *pVal)
{
    struct osdThreadPrivate *pPvt = (struct osdThreadPrivate *) id;
    BOOL stat = TlsSetValue (pPvt->key, (void *) pVal );
    assert (stat);
}

/*
 * threadPrivateGet ()
 */
epicsShareFunc void * epicsShareAPI threadPrivateGet (threadPrivateId id)
{
    struct osdThreadPrivate *pPvt = (struct osdThreadPrivate *) id;
    return (void *) TlsGetValue (pPvt->key);
}

#ifdef TEST_CODES
void testPriorityMapping ()
{
    unsigned i;

    for (i=threadPriorityMin; i<threadPriorityMax; i++) {
        printf ("%u %d\n", i, threadGetOsdPriorityValue (i) );
    }

    for (i=0; i<osdPriorityStateCount; i++) {
        printf ("%d %u\n", osdPriorityList[i], threadGetOsiPriorityValue(osdPriorityList[i]));
    }
    return 0;
}
#endif