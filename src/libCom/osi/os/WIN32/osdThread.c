
/*
 * $Id$
 *
 * Author: Jeff Hill
 * 
 *
 *
 */

#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

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
    int priority;
    DWORD id;
} win32ThreadParam;

static DWORD tlsIndexWIN32 = 0xFFFFFFFF;

/* 
 * threadGetOsiPriorityValue ()
 */
epicsShareFunc int epicsShareAPI threadGetOsiPriorityValue (int osdPriority) 
{ 
    int osiPriority;

    if (osdPriority<THREAD_BASE_PRIORITY_IDLE) {
        return threadPriorityMin;
    }

    if (osdPriority>THREAD_PRIORITY_TIME_CRITICAL) {
        return threadPriorityMax;
    }

    osiPriority = osdPriority - THREAD_BASE_PRIORITY_IDLE;
    osiPriority *= threadPriorityMax - threadPriorityMin;
    osiPriority /= THREAD_PRIORITY_TIME_CRITICAL - THREAD_BASE_PRIORITY_IDLE;
    osiPriority += threadPriorityMin;
    return osiPriority;
}

/*
 * threadGetOssPriorityValue ()
 */
epicsShareFunc int epicsShareAPI threadGetOssPriorityValue (unsigned osiPriority) 
{
    unsigned osdPriority;

    if (osiPriority<threadPriorityMin) {
        return THREAD_BASE_PRIORITY_IDLE;
    }

    if (osiPriority>threadPriorityMax) {
        return THREAD_PRIORITY_TIME_CRITICAL;
    }

    osdPriority = osiPriority - threadPriorityMin;
    osdPriority *= THREAD_PRIORITY_TIME_CRITICAL - THREAD_BASE_PRIORITY_IDLE;
    osdPriority /= threadPriorityMax - threadPriorityMin;
    osdPriority += THREAD_BASE_PRIORITY_IDLE;
    return osdPriority;
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
static DWORD WINAPI epicsWin32ThreadEntry (LPVOID lpParameter)
{
    win32ThreadParam *pParm = (win32ThreadParam *) lpParameter;
    BOOL stat;

    stat = TlsSetValue (tlsIndexWIN32, pParm);
    if (stat) {
        return 0;
    }

    (*pParm->funptr) (pParm->parm);

    CloseHandle (pParm->handle); 
    free (pParm->pName);
    free (pParm);

    return 1;
}

/*
 * freeThreadLocalStorageIndex ()
 */
static void freeThreadLocalStorageIndex (void)
{
    if (tlsIndexWIN32!=0xFFFFFFFF) {
        TlsFree (tlsIndexWIN32);
    }
}
 
/*
 * threadCreate()
 */
epicsShareFunc threadId epicsShareAPI threadCreate (const char *pName,
    unsigned int priority, unsigned int stackSize, THREADFUNC pFunc,void *pParm)
{
    win32ThreadParam *pParmWIN32;
    DWORD wstat;
    BOOL bstat;

    if (tlsIndexWIN32==0xFFFFFFFF) {
        tlsIndexWIN32 = TlsAlloc();
        if (tlsIndexWIN32==0xFFFFFFFF) {
            return NULL;
        }
        else {
            int status;
            status = atexit (freeThreadLocalStorageIndex);
            if (status) {
                TlsFree (tlsIndexWIN32);
                tlsIndexWIN32 = 0xFFFFFFFF;
                return NULL;
            }
        }
    }
 
    pParmWIN32 = malloc (sizeof(*pParmWIN32));
    if (pParmWIN32==NULL) {
        return NULL;
    }

    pParmWIN32->pName = malloc (strlen(pName)+1);
    if (pParmWIN32->pName==NULL) {
        free (pParmWIN32);
        return NULL;
    }
    strcpy (pParmWIN32->pName, pName);

    pParmWIN32->funptr = pFunc;
    pParmWIN32->parm = pParm;
    pParmWIN32->priority = threadGetOssPriorityValue (priority);
    pParmWIN32->handle = CreateThread (NULL, stackSize, epicsWin32ThreadEntry, 
                                pParmWIN32, CREATE_SUSPENDED , &pParmWIN32->id);
    if (pParmWIN32->handle==NULL) {
        free (pParmWIN32->pName);
        free (pParmWIN32);
        return NULL;
    }

    bstat = SetThreadPriority (pParmWIN32->handle, pParmWIN32->priority);
    if (!bstat) {
        CloseHandle (pParmWIN32->handle); 
        free (pParmWIN32->pName);
        free (pParmWIN32);
        return NULL;
    }
 
    wstat =  ResumeThread (pParmWIN32->handle);
    if (wstat==0xFFFFFFFF) {
        CloseHandle (pParmWIN32->handle); 
        free (pParmWIN32->pName);
        free (pParmWIN32);
        return NULL;
    }

    return (threadId) pParmWIN32;
}

/*
 * threadDestroy ()
 */
epicsShareFunc void epicsShareAPI threadDestroy (threadId id) 
{ 
    win32ThreadParam *pParm = (win32ThreadParam *) id;
    BOOL stat;

    stat = TerminateThread (pParm->handle, 0);
    assert (stat);

    CloseHandle (pParm->handle); 
    free (pParm->pName);
    free (pParm);

    printf ("threadDestroy() appears to be reckless on windows\n");
}

/*
 * threadSuspend ()
 */
epicsShareFunc void epicsShareAPI threadSuspend (threadId id)
{
    win32ThreadParam *pParm = (win32ThreadParam *) id;
    DWORD stat;

    stat =  SuspendThread (pParm->handle);
    assert (stat!=0xFFFFFFFF);
}

/*
 * threadResume ()
 */
epicsShareFunc void epicsShareAPI threadResume (threadId id)
{
    win32ThreadParam *pParm = (win32ThreadParam *) id;
    DWORD stat;

    stat =  ResumeThread (pParm->handle);
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
 * threadSetPriority ()
 */
epicsShareFunc void epicsShareAPI threadSetPriority (threadId id, unsigned priority) 
{
    win32ThreadParam *pParm = (win32ThreadParam *) id;
    BOOL stat;

    stat = SetThreadPriority (pParm->handle, threadGetOssPriorityValue(priority));
    assert (stat);
}

/*
 * threadSetDestroySafe ()
 */
epicsShareFunc void epicsShareAPI threadSetDestroySafe () 
{
    /*
     * no matching windows functionality ?
     */
    printf ("unable to create functionally correct threadSetDestroySafe() on win32\n");
}

/*
 * threadSetDestroyUnsafe ()
 */
epicsShareFunc void epicsShareAPI threadSetDestroyUnsafe () 
{
    /*
     * no matching windows functionality ?
     */
    printf ("unable to create functionally correct threadSetDestroyUnsafe() on win32\n");
}

/*
 * threadGetName ()
 */
epicsShareFunc const char * epicsShareAPI threadGetName (threadId id) 
{
    win32ThreadParam *pParm = (win32ThreadParam *) id;

    return pParm->pName;
}

/*
 * threadIsEqual ()
 */
epicsShareFunc int epicsShareAPI threadIsEqual (threadId id1, threadId id2) 
{
    win32ThreadParam *pParm1 = (win32ThreadParam *) id1;
    win32ThreadParam *pParm2 = (win32ThreadParam *) id2;

    return ( pParm1->id == pParm2->id );
}

/*
 * threadIsReady ()
 */
epicsShareFunc int epicsShareAPI threadIsReady (threadId id) 
{
    printf ("unable to create functionally correct threadIsReady() on win32\n");
    return 1; 
}

/*
 * threadIsSuspended ()
 */
epicsShareFunc int epicsShareAPI threadIsSuspended (threadId id) 
{
    printf ("unable to create functionally correct threadIsSuspended() on win32\n");
    return 0; 
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
    return (threadId) TlsGetValue (tlsIndexWIN32); 
}

/*
 * threadLockContextSwitch ()
 */
epicsShareFunc void epicsShareAPI threadLockContextSwitch (void) 
{
    win32ThreadParam *pParm;
    BOOL stat;

    pParm = (win32ThreadParam *) TlsGetValue (tlsIndexWIN32);
    assert (pParm);

    stat = SetThreadPriority (pParm->handle, THREAD_PRIORITY_TIME_CRITICAL);
    assert (stat);

    printf ("I doubt that this is a correct implementation of threadLockContextSwitch()\n");
}

/*
 * threadUnlockContextSwitch ()
 */
epicsShareFunc void epicsShareAPI threadUnlockContextSwitch (void) 
{
    win32ThreadParam *pParm;
    BOOL stat;

    pParm = (win32ThreadParam *) TlsGetValue (tlsIndexWIN32);
    assert (pParm);

    stat = SetThreadPriority (pParm->handle, pParm->priority);
    assert (stat);

    printf ("I doubt that this is a correct implementation of threadUnlockContextSwitch()\n");
}

/*
 * threadNameToId ()
 */
epicsShareFunc threadId epicsShareAPI threadNameToId (const char *name) 
{ 
    printf ("threadNameToId() isnt implemented\n");
    return 0; 
}
