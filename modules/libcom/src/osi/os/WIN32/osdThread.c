/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author: Jeff Hill
 */

/* pull in _WIN32_WINNT* definitions, if _WIN32_WINNT
 * is not already defined it is set to max supported by SDK
 */
#include <sdkddkver.h>

#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#define VC_EXTRALEAN
#define STRICT
#include <windows.h>
#include <process.h> /* for _endthread() etc */

#include "epicsStdio.h"
#include "libComAPI.h"
#include "epicsThread.h"
#include "cantProceed.h"
#include "epicsAssert.h"
#include "ellLib.h"
#include "epicsExit.h"
#include "epicsAtomic.h"
#include "osdThreadPvt.h"

/* Windows Vista and higher supports fibre functions, but the
 * prototypes only appear in the windows SDK 8 and above.
 * VS2010 supplies sdk 7, but can be upgraded to later SDK 

 * To accomodate this we suuply prototypes on, for XP
 * fall back to Tls*() which will build and run
 * correctly for epicsThreads, but means that TLS allocations from
 * epicsThreadImplicitCreate() will continue to leak (for non-EPICS threads).
 *
 * Also, WINE circa 5.0.3 provides the FLS storage functions, but doesn't
 * actually run the dtor function.
 *
 * we check for existance of _WIN32_WINNT_WIN8 which will only be defined
 * in SDK 8 and above. If Visa is detected and SDK < 8 we will supply
 * the missing prototypes
 */

#if _WIN32_WINNT >= 0x0600 /* VISTA */
#   ifdef _WIN32_WINNT_WIN8 /* Existance means using SDK 8 or higher */
#       include <fibersapi.h>
#   else
#       include <winnt.h> /* for PFLS_CALLBACK_FUNCTION */
/* the fibers api exists in vista and above, but no header is provided until SDK 8 */
        WINBASEAPI DWORD WINAPI FlsAlloc(PFLS_CALLBACK_FUNCTION);
        WINBASEAPI PVOID WINAPI FlsGetValue(DWORD);
        WINBASEAPI BOOL WINAPI FlsSetValue(DWORD , PVOID);
        WINBASEAPI BOOL WINAPI FlsFree(DWORD);
#   endif
#elif _WIN32_WINNT >= 0x0501 /* Windows XP */
    typedef void (WINAPI *xPFLS_CALLBACK_FUNCTION) (void*);
    static
    DWORD xFlsAlloc(xPFLS_CALLBACK_FUNCTION dtor) {
        (void)dtor;
        return TlsAlloc();
    }
#   define FlsAlloc xFlsAlloc
#   define FlsSetValue TlsSetValue
#   define FlsGetValue TlsGetValue
#   define USE_TLSALLOC_FALLBACK
#else
#   error Minimum supported is Windows XP
#endif

LIBCOM_API void osdThreadHooksRun(epicsThreadId id);

void setThreadName ( DWORD dwThreadID, LPCSTR szThreadName );
static void WINAPI epicsParmCleanupWIN32 ( void * praw );

typedef struct win32ThreadGlobal {
    CRITICAL_SECTION mutex;
    ELLLIST threadList;
    DWORD flsIndexThreadLibraryEPICS;
} win32ThreadGlobal;

typedef struct epicsThreadOSD {
    ELLNODE node;
    int refcnt;
    HANDLE handle;
    EPICSTHREADFUNC funptr;
    void * parm;
    char * pName;
    DWORD id;
    unsigned epicsPriority;
    char isSuspended;
    int joinable;
    HANDLE timer; /* waitable timer */
} win32ThreadParam;

typedef struct epicsThreadPrivateOSD {
    DWORD key;
} epicsThreadPrivateOSD;

#ifndef STACK_SIZE_PARAM_IS_A_RESERVATION
#   define STACK_SIZE_PARAM_IS_A_RESERVATION 0x00010000
#endif

#define osdOrdinaryPriorityStateCount 5u
static const int osdOrdinaryPriorityList [osdOrdinaryPriorityStateCount] =
{
    THREAD_PRIORITY_LOWEST,       /* -2 on >= W2K ??? on W95 */
    THREAD_PRIORITY_BELOW_NORMAL, /* -1 on >= W2K ??? on W95 */
    THREAD_PRIORITY_NORMAL,       /*  0 on >= W2K ??? on W95 */
    THREAD_PRIORITY_ABOVE_NORMAL, /*  1 on >= W2K ??? on W95 */
    THREAD_PRIORITY_HIGHEST       /*  2 on >= W2K ??? on W95 */
};

#   define osdRealtimePriorityStateCount 14u
static const int osdRealtimePriorityList [osdRealtimePriorityStateCount] =
{
    -7, /* allowed on >= W2k, but no #define supplied */
    -6, /* allowed on >= W2k, but no #define supplied */
    -5, /* allowed on >= W2k, but no #define supplied */
    -4, /* allowed on >= W2k, but no #define supplied */
    -3, /* allowed on >= W2k, but no #define supplied */
    THREAD_PRIORITY_LOWEST,       /* -2 on >= W2K ??? on W95 */
    THREAD_PRIORITY_BELOW_NORMAL, /* -1 on >= W2K ??? on W95 */
    THREAD_PRIORITY_NORMAL,       /*  0 on >= W2K ??? on W95 */
    THREAD_PRIORITY_ABOVE_NORMAL, /*  1 on >= W2K ??? on W95 */
    THREAD_PRIORITY_HIGHEST,      /*  2 on >= W2K ??? on W95 */
    3, /* allowed on >= W2k, but no #define supplied */
    4, /* allowed on >= W2k, but no #define supplied */
    5, /* allowed on >= W2k, but no #define supplied */
    6  /* allowed on >= W2k, but no #define supplied */
};

#if defined(EPICS_BUILD_DLL)
BOOL WINAPI DllMain (
    HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved )
{
    static DWORD dllHandleIndex;
    HMODULE dllHandle = 0;
    BOOL success = TRUE;

    switch ( dwReason )
    {
    case DLL_PROCESS_ATTACH:
        dllHandleIndex = TlsAlloc ();
        if ( dllHandleIndex == TLS_OUT_OF_INDEXES ) {
            success = FALSE;
        }
        break;

    case DLL_PROCESS_DETACH:
        success = TlsFree ( dllHandleIndex );
        break;

    case DLL_THREAD_ATTACH:
        /*
         * Don't allow user's explicitly calling FreeLibrary for Com.dll to yank
         * the carpet out from under EPICS threads that are still using Com.dll
         */
        /*
         * Only in WXP
         * That's a shame because this is probably much faster
         */
        success = GetModuleHandleEx (
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            ( LPCTSTR ) DllMain, & dllHandle );

        if ( success ) {
            success = TlsSetValue ( dllHandleIndex, dllHandle );
        }
        break;
    case DLL_THREAD_DETACH:
        /*
         * Thread is exiting, release Com.dll. I am assuming that windows is
         * smart enough to postpone the unload until this function returns.
         */
        dllHandle = TlsGetValue ( dllHandleIndex );
        if ( dllHandle ) {
            success = FreeLibrary ( dllHandle );
        }
        break;
    }

    return success;
}
#endif

/*
 * fetchWin32ThreadGlobal ()
 * Search for "Synchronization and Multiprocessor Issues" in ms doc
 * to understand why this is necessary and why this works on smp systems.
 */
static win32ThreadGlobal * fetchWin32ThreadGlobal ( void )
{
    static win32ThreadGlobal * pWin32ThreadGlobal = 0;
    static LONG initStarted = 0;
    static LONG initCompleted = 0;
    LONG started;
    LONG done;

    done = InterlockedCompareExchange ( & initCompleted, 0, 0 );
    if ( done ) {
        return pWin32ThreadGlobal;
    }

    started = InterlockedCompareExchange ( & initStarted, 0, 1 );
    if ( started ) {
        unsigned tries = 0u;
        while ( ! InterlockedCompareExchange ( & initCompleted, 0, 0 ) ) {
            /*
             * I am not fond of busy loops, but since this will
             * collide very infrequently and this is the lowest
             * level init then perhaps this is ok
             */
            Sleep ( 1 );
            if ( tries++ > 1000 ) {
                return 0;
            }
        }
        return pWin32ThreadGlobal;
    }

    pWin32ThreadGlobal = ( win32ThreadGlobal * )
        calloc ( 1, sizeof ( * pWin32ThreadGlobal ) );
    if ( ! pWin32ThreadGlobal ) {
        InterlockedExchange ( & initStarted, 0 );
        return 0;
    }

    InitializeCriticalSection ( & pWin32ThreadGlobal->mutex );
    ellInit ( & pWin32ThreadGlobal->threadList );
    pWin32ThreadGlobal->flsIndexThreadLibraryEPICS = FlsAlloc(&epicsParmCleanupWIN32);
    if ( pWin32ThreadGlobal->flsIndexThreadLibraryEPICS == FLS_OUT_OF_INDEXES ) {
        DeleteCriticalSection ( & pWin32ThreadGlobal->mutex );
        free ( pWin32ThreadGlobal );
        pWin32ThreadGlobal = 0;
        return 0;
    }

    InterlockedExchange ( & initCompleted, 1 );

    return pWin32ThreadGlobal;
}

static void epicsParmCleanupDataWIN32 ( win32ThreadParam * pParm )
{
    if ( pParm ) {
        if ( pParm->handle ) {
            CloseHandle ( pParm->handle );
        }
        if ( pParm->timer ) {
            CloseHandle ( pParm->timer );
        }
        free ( pParm );
    }
}

static void WINAPI epicsParmCleanupWIN32 ( void * praw )
{
    win32ThreadParam * pParm = praw;
    win32ThreadGlobal * pGbl;
    if(!praw)
        return;

    if ( ! (pGbl = fetchWin32ThreadGlobal ()) )  {
        fprintf ( stderr, "epicsParmCleanupWIN32: unable to find ctx\n" );
        return;
    }

    if ( pParm ) {
        int cnt = epicsAtomicDecrIntT(&pParm->refcnt);
        if(cnt > 0) return;
        assert(cnt==0);

        /* fprintf ( stderr, "thread %s is exiting\n", pParm->pName ); */
        EnterCriticalSection ( & pGbl->mutex );
        ellDelete ( & pGbl->threadList, & pParm->node );
        LeaveCriticalSection ( & pGbl->mutex );

        epicsParmCleanupDataWIN32 ( pParm );
    }
}

/*
 * epicsThreadExitMain ()
 */
LIBCOM_API void epicsStdCall epicsThreadExitMain ( void )
{
    cantProceed("epicsThreadExitMain() must no longer be used.\n");
}

/*
 * osdPriorityMagFromPriorityOSI ()
 */
static unsigned osdPriorityMagFromPriorityOSI ( unsigned osiPriority, unsigned priorityStateCount )
{
    unsigned magnitude;

    /* optimizer will remove this one if epicsThreadPriorityMin is zero */
    /* and osiPriority is unsigned */
    if ( osiPriority < epicsThreadPriorityMin ) {
        osiPriority = epicsThreadPriorityMin;
    }

    if ( osiPriority > epicsThreadPriorityMax ) {
        osiPriority = epicsThreadPriorityMax;
    }

    magnitude = osiPriority * priorityStateCount;
    magnitude /= ( epicsThreadPriorityMax - epicsThreadPriorityMin ) + 1;

    return magnitude;
}

LIBCOM_API
void epicsThreadRealtimeLock(void)
{}

/*
 * epicsThreadGetOsdPriorityValue ()
 */
static int epicsThreadGetOsdPriorityValue ( unsigned osiPriority )
{
    const DWORD priorityClass = GetPriorityClass ( GetCurrentProcess () );
    const int * pStateList;
    unsigned stateCount;
    unsigned magnitude;

    if ( priorityClass == REALTIME_PRIORITY_CLASS ) {
        stateCount = osdRealtimePriorityStateCount;
        pStateList = osdRealtimePriorityList;
    }
    else {
        stateCount = osdOrdinaryPriorityStateCount;
        pStateList = osdOrdinaryPriorityList;
    }

    magnitude = osdPriorityMagFromPriorityOSI ( osiPriority, stateCount );
    return pStateList[magnitude];
}

/*
 * osiPriorityMagFromMagnitueOSD ()
 */
static unsigned osiPriorityMagFromMagnitueOSD ( unsigned magnitude, unsigned osdPriorityStateCount )
{
    unsigned osiPriority;

    osiPriority = magnitude * ( epicsThreadPriorityMax - epicsThreadPriorityMin );
    osiPriority /= osdPriorityStateCount - 1u;
    osiPriority += epicsThreadPriorityMin;

    return osiPriority;
}


/*
 * epicsThreadGetOsiPriorityValue ()
 */
static unsigned epicsThreadGetOsiPriorityValue ( int osdPriority )
{
    const DWORD priorityClass = GetPriorityClass ( GetCurrentProcess () );
    const int * pStateList;
    unsigned stateCount;
    unsigned magnitude;

    if ( priorityClass == REALTIME_PRIORITY_CLASS ) {
        stateCount = osdRealtimePriorityStateCount;
        pStateList = osdRealtimePriorityList;
    }
    else {
        stateCount = osdOrdinaryPriorityStateCount;
        pStateList = osdOrdinaryPriorityList;
    }

    for ( magnitude = 0u; magnitude < stateCount; magnitude++ ) {
        if ( osdPriority == pStateList[magnitude] ) {
            break;
        }
    }

    if ( magnitude >= stateCount ) {
        fprintf ( stderr,
            "Unrecognized WIN32 thread priority level %d.\n",
            osdPriority );
        fprintf ( stderr,
            "Mapping to EPICS thread priority level epicsThreadPriorityMin.\n" );
        return epicsThreadPriorityMin;
    }

    return osiPriorityMagFromMagnitueOSD ( magnitude, stateCount );
}

/*
 * epicsThreadLowestPriorityLevelAbove ()
 */
LIBCOM_API epicsThreadBooleanStatus epicsStdCall epicsThreadLowestPriorityLevelAbove 
            ( unsigned int priority, unsigned * pPriorityJustAbove )
{
    const DWORD priorityClass = GetPriorityClass ( GetCurrentProcess () );
    epicsThreadBooleanStatus status;
    unsigned stateCount;
    unsigned magnitude;

    if ( priorityClass == REALTIME_PRIORITY_CLASS ) {
        stateCount = osdRealtimePriorityStateCount;
    }
    else {
        stateCount = osdOrdinaryPriorityStateCount;
    }

    magnitude = osdPriorityMagFromPriorityOSI ( priority, stateCount );

    if ( magnitude < ( stateCount - 1 ) ) {
        *pPriorityJustAbove = osiPriorityMagFromMagnitueOSD ( magnitude + 1u, stateCount );
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
LIBCOM_API epicsThreadBooleanStatus epicsStdCall epicsThreadHighestPriorityLevelBelow 
            ( unsigned int priority, unsigned * pPriorityJustBelow )
{
    const DWORD priorityClass = GetPriorityClass ( GetCurrentProcess () );
    epicsThreadBooleanStatus status;
    unsigned stateCount;
    unsigned magnitude;

    if ( priorityClass == REALTIME_PRIORITY_CLASS ) {
        stateCount = osdRealtimePriorityStateCount;
    }
    else {
        stateCount = osdOrdinaryPriorityStateCount;
    }

    magnitude = osdPriorityMagFromPriorityOSI ( priority, stateCount );

    if ( magnitude > 0u ) {
        *pPriorityJustBelow = osiPriorityMagFromMagnitueOSD ( magnitude - 1u, stateCount );
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
LIBCOM_API unsigned int epicsStdCall 
    epicsThreadGetStackSize ( epicsThreadStackSizeClass stackSizeClass )
{
    #define STACK_SIZE(f) (f * 0x10000 * sizeof(void *))
    static const unsigned stackSizeTable[epicsThreadStackBig+1] = {
        STACK_SIZE(1), STACK_SIZE(2), STACK_SIZE(4)
    };

    if (stackSizeClass<epicsThreadStackSmall) {
        fprintf ( stderr,
            "epicsThreadGetStackSize illegal argument (too small)");
        return stackSizeTable[epicsThreadStackBig];
    }

    if (stackSizeClass>epicsThreadStackBig) {
        fprintf ( stderr,
            "epicsThreadGetStackSize illegal argument (too large)");
        return stackSizeTable[epicsThreadStackBig];
    }

    return stackSizeTable[stackSizeClass];
}

/*
 * epicsWin32ThreadEntry()
 */
static unsigned WINAPI epicsWin32ThreadEntry ( LPVOID lpParameter )
{
    win32ThreadGlobal * pGbl = fetchWin32ThreadGlobal ();
    win32ThreadParam * pParm = ( win32ThreadParam * ) lpParameter;
    unsigned retStat = 0u;
    BOOL success;

    if ( pGbl )  {
        setThreadName ( pParm->id, pParm->pName );

        success = FlsSetValue ( pGbl->flsIndexThreadLibraryEPICS, pParm );
        if ( success ) {
            osdThreadHooksRun ( ( epicsThreadId ) pParm );
            /* printf ( "starting thread %d\n", pParm->id ); */
            ( *pParm->funptr ) ( pParm->parm );
            /* printf ( "terminating thread %d\n", pParm->id ); */
            retStat = 1;
        }
        else {
            fprintf ( stderr, "epicsWin32ThreadEntry: unable to set private\n" );
        }
    }
    else {
        fprintf ( stderr, "epicsWin32ThreadEntry: unable to find ctx\n" );
    }

    epicsExitCallAtThreadExits ();

    /* On Windows we could omit this and rely on the callback given to FlsAlloc() to free.
     * However < vista doesn't implement FLS at all, and WINE (circa 5.0.3) doesn't
     * implement fully (dtor never runs).  So for EPICS threads, we explicitly
     * free() here.
     */
    if(pGbl && FlsSetValue ( pGbl->flsIndexThreadLibraryEPICS, NULL )) {
        epicsParmCleanupWIN32 ( pParm );
    }

    return retStat; /* this indirectly closes the thread handle */
}

static win32ThreadParam * epicsThreadParmCreate ( const char *pName )
{
    win32ThreadParam *pParmWIN32;

    pParmWIN32 = calloc ( 1, sizeof ( *pParmWIN32 ) + strlen ( pName ) + 1 );
    if ( pParmWIN32  ) {
        pParmWIN32->pName = (char *) ( pParmWIN32 + 1 );
        strcpy ( pParmWIN32->pName, pName );
        pParmWIN32->isSuspended = 0;
        epicsAtomicIncrIntT(&pParmWIN32->refcnt);
#ifdef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
        pParmWIN32->timer = CreateWaitableTimerEx(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
#endif
        if (pParmWIN32->timer == NULL) {
            pParmWIN32->timer = CreateWaitableTimer(NULL, 0, NULL);
        }
        if (pParmWIN32->timer == NULL) {
            free(pParmWIN32);
            return NULL;
        }
    }
    return pParmWIN32;
}

static win32ThreadParam * epicsThreadImplicitCreate ( void )
{
    win32ThreadGlobal * pGbl = fetchWin32ThreadGlobal ();
    DWORD id = GetCurrentThreadId ();
    win32ThreadParam * pParm;
    char name[64];
    HANDLE handle;
    BOOL success;

    if ( ! pGbl )  {
        fprintf ( stderr, "epicsThreadImplicitCreate: unable to find ctx\n" );
        return 0;
    }

    success = DuplicateHandle ( GetCurrentProcess (), GetCurrentThread (),
            GetCurrentProcess (), & handle, 0, FALSE, DUPLICATE_SAME_ACCESS );
    if ( ! success ) {
        return 0;
    }
    {
        unsigned long idForFormat = id;
        sprintf ( name, "win%lx", idForFormat );
    }
    pParm = epicsThreadParmCreate ( name );
    if ( pParm ) {
        int win32ThreadPriority;

        pParm->handle = handle;
        pParm->id = id;
        win32ThreadPriority = GetThreadPriority ( pParm->handle );
        assert ( win32ThreadPriority != THREAD_PRIORITY_ERROR_RETURN );
        pParm->epicsPriority = epicsThreadGetOsiPriorityValue ( win32ThreadPriority );
        success = FlsSetValue ( pGbl->flsIndexThreadLibraryEPICS, pParm );
        if ( ! success ) {
            epicsParmCleanupWIN32 ( pParm );
            pParm = 0;
        }
        else {
            EnterCriticalSection ( & pGbl->mutex );
            ellAdd ( & pGbl->threadList, & pParm->node );
            LeaveCriticalSection ( & pGbl->mutex );
        }
    }
    return pParm;
}

/*
 * epicsThreadCreate ()
 */
epicsThreadId epicsThreadCreateOpt (
    const char * pName,
    EPICSTHREADFUNC pFunc, void * pParm,
    const epicsThreadOpts *opts )
{
    win32ThreadGlobal * pGbl = fetchWin32ThreadGlobal ();
    win32ThreadParam * pParmWIN32;
    unsigned int stackSize;
    int osdPriority;
    DWORD wstat;
    BOOL bstat;

    if ( ! pGbl )  {
        return NULL;
    }

    if (!opts) {
        static const epicsThreadOpts opts_default = EPICS_THREAD_OPTS_INIT;
        opts = &opts_default;
    }
    stackSize = opts->stackSize;
    if (stackSize <= epicsThreadStackBig)
        stackSize = epicsThreadGetStackSize(stackSize);

    pParmWIN32 = epicsThreadParmCreate ( pName );
    if ( pParmWIN32 == 0 ) {
        return ( epicsThreadId ) pParmWIN32;
    }
    pParmWIN32->funptr = pFunc;
    pParmWIN32->parm = pParm;
    pParmWIN32->epicsPriority = opts->priority;
    if(opts->joinable) {
        pParmWIN32->joinable = 1;
        epicsAtomicIncrIntT(&pParmWIN32->refcnt);
    }

    {
        unsigned threadId;
        pParmWIN32->handle = (HANDLE) _beginthreadex (
            0, stackSize, epicsWin32ThreadEntry,
            pParmWIN32,
            CREATE_SUSPENDED | STACK_SIZE_PARAM_IS_A_RESERVATION,
            & threadId );
        if ( pParmWIN32->handle == 0 ) {
            epicsParmCleanupDataWIN32 ( pParmWIN32 );
            return NULL;
        }
        /* weird win32 interface threadId parameter inconsistency */
        pParmWIN32->id = ( DWORD ) threadId ;
    }

    osdPriority = epicsThreadGetOsdPriorityValue (opts->priority);
    bstat = SetThreadPriority ( pParmWIN32->handle, osdPriority );
    if (!bstat) {
        epicsParmCleanupDataWIN32 ( pParmWIN32 );
        return NULL;
    }

    EnterCriticalSection ( & pGbl->mutex );
    ellAdd ( & pGbl->threadList, & pParmWIN32->node );
    LeaveCriticalSection ( & pGbl->mutex );

    wstat =  ResumeThread ( pParmWIN32->handle );
    if (wstat==0xFFFFFFFF) {
        EnterCriticalSection ( & pGbl->mutex );
        ellDelete ( & pGbl->threadList, & pParmWIN32->node );
        LeaveCriticalSection ( & pGbl->mutex );
        epicsParmCleanupDataWIN32 ( pParmWIN32 );
        return NULL;
    }

    /* after this point, new thread is responsible to free(pParmWIN32) */

    return ( epicsThreadId ) pParmWIN32;
}

void epicsThreadMustJoin(epicsThreadId id)
{
    win32ThreadParam * pParmWIN32 = id;

    if(!id) {
        /* no-op */
    } else if(epicsAtomicCmpAndSwapIntT(&id->joinable, 1, 0)!=1) {
        if(epicsThreadGetIdSelf()==id) {
            fprintf(stderr, "Warning: %s thread self-join of unjoinable\n", pParmWIN32->pName);

        } else {
            /* try to error nicely, however in all likelihood de-ref of
             * 'id' has already caused SIGSEGV as we are racing thread exit,
             * which free's 'id'.
             */
            cantProceed("Error: %s thread not joinable.\n", pParmWIN32->pName);
        }

    } else if(epicsThreadGetIdSelf() != id) {
        DWORD status = WaitForSingleObject(pParmWIN32->handle, INFINITE);
        if(status != WAIT_OBJECT_0) {
            /* TODO: signal error? */
        }

        epicsParmCleanupWIN32(pParmWIN32);
    } else {
        /* join self silently does nothing */
        epicsParmCleanupWIN32(pParmWIN32);
    }
}

/*
 * getMyWin32ThreadParam ()
 */
static void* getMyWin32ThreadParam ( win32ThreadGlobal * pGbl )
{
    win32ThreadParam * pParm;

    if ( ! pGbl ) {
        pGbl = fetchWin32ThreadGlobal ();
        assert ( pGbl );
    }

    pParm = ( win32ThreadParam * )
        FlsGetValue ( pGbl->flsIndexThreadLibraryEPICS );
    if ( ! pParm ) {
        pParm = epicsThreadImplicitCreate ();
    }
    return pParm;
}

/*
 * epicsThreadSuspendSelf ()
 */
LIBCOM_API void epicsStdCall epicsThreadSuspendSelf ()
{
    DWORD stat;
    win32ThreadGlobal * pGbl = fetchWin32ThreadGlobal ();
    win32ThreadParam * pParm = getMyWin32ThreadParam ( pGbl );

    if ( pParm ) {
        EnterCriticalSection ( & pGbl->mutex );
        pParm->isSuspended = 1;
        LeaveCriticalSection ( & pGbl->mutex );
    }
    stat =  SuspendThread ( GetCurrentThread () );
    assert ( stat != 0xFFFFFFFF );
}

/*
 * epicsThreadResume ()
 */
LIBCOM_API void epicsStdCall epicsThreadResume ( epicsThreadId id )
{
    win32ThreadGlobal * pGbl = fetchWin32ThreadGlobal ();
    win32ThreadParam * pParm = ( win32ThreadParam * ) id;
    DWORD stat;

    assert ( pGbl );

    EnterCriticalSection ( & pGbl->mutex );

    stat =  ResumeThread ( pParm->handle );
    pParm->isSuspended = 0;

    LeaveCriticalSection ( & pGbl->mutex );

    assert ( stat != 0xFFFFFFFF );
}

/*
 * epicsThreadGetPriority ()
 */
LIBCOM_API unsigned epicsStdCall epicsThreadGetPriority (epicsThreadId id) 
{
    win32ThreadParam * pParm = ( win32ThreadParam * ) id;
    return pParm->epicsPriority;
}

/*
 * epicsThreadGetPrioritySelf ()
 */
LIBCOM_API unsigned epicsStdCall epicsThreadGetPrioritySelf () 
{ 
    win32ThreadParam * pParm = getMyWin32ThreadParam ( NULL );

    if ( pParm ) {
        return pParm->epicsPriority;
    }
    else {
        int win32ThreadPriority =
            GetThreadPriority ( GetCurrentThread () );
        assert ( win32ThreadPriority != THREAD_PRIORITY_ERROR_RETURN );
        return epicsThreadGetOsiPriorityValue ( win32ThreadPriority );
    }
}

/*
 * epicsThreadSetPriority ()
 */
LIBCOM_API void epicsStdCall epicsThreadSetPriority ( epicsThreadId id, unsigned priority ) 
{
    win32ThreadParam * pParm = ( win32ThreadParam * ) id;
    BOOL stat = SetThreadPriority ( pParm->handle, epicsThreadGetOsdPriorityValue (priority) );
    assert (stat);
}

/*
 * epicsThreadIsEqual ()
 */
LIBCOM_API int epicsStdCall epicsThreadIsEqual ( epicsThreadId id1, epicsThreadId id2 ) 
{
    win32ThreadParam * pParm1 = ( win32ThreadParam * ) id1;
    win32ThreadParam * pParm2 = ( win32ThreadParam * ) id2;
    return ( id1 == id2 && pParm1->id == pParm2->id );
}

/*
 * epicsThreadIsSuspended ()
 */
LIBCOM_API int epicsStdCall epicsThreadIsSuspended ( epicsThreadId id ) 
{
    win32ThreadParam *pParm = ( win32ThreadParam * ) id;
    DWORD exitCode;
    BOOL stat;

    stat = GetExitCodeThread ( pParm->handle, & exitCode );
    if ( stat ) {
        if ( exitCode != STILL_ACTIVE ) {
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

/**
 * osdThreadGetTimer ()
 * return stored waitable timer object for thread
 */
HANDLE osdThreadGetTimer()
{
    win32ThreadParam * pParm = getMyWin32ThreadParam ( NULL );
    if (pParm) {
        return pParm->timer;
    } else {
        return NULL;
    }
}

/*
 * epicsThreadSleep ()
 */
LIBCOM_API void epicsStdCall epicsThreadSleep ( double seconds )
{
    /* waitable timers use 100 nanosecond intervals, like FILETIME */
    static const unsigned ivalPerSec = 10000000u; /* number of 100ns intervals per second */
    static const unsigned mSecPerSec = 1000u;     /* milliseconds per second */
    LARGE_INTEGER tmo;
    HANDLE timer;
    LONGLONG nIvals; /* number of intervals */

    if ( seconds <= 0.0 ) {
        tmo.QuadPart = 0u;
    }
    else if ( seconds >= INFINITE / mSecPerSec  ) { 
        /* we need to apply a maximum wait time to stop an overflow. We choose (INFINITE - 1) milliseconds,
           to be compatible with previous WaitForSingleObject() implementation */    
        nIvals = (LONGLONG)(INFINITE - 1) * (ivalPerSec / mSecPerSec);
        tmo.QuadPart = -nIvals; /* negative value means a relative time offset for timer */
    }
    else {
        nIvals = (LONGLONG)(seconds * ivalPerSec + 0.999999);
        tmo.QuadPart = -nIvals;
    }

    if (tmo.QuadPart == 0) {
        Sleep ( 0 );
    }
    else {
        timer = osdThreadGetTimer();
        if (!SetWaitableTimer(timer, &tmo, 0, NULL, NULL, 0)) {
            fprintf ( stderr, "epicsThreadSleep: SetWaitableTimer failed %lu\n", GetLastError() );
            return;
        }
        if (WaitForSingleObject(timer, INFINITE) != WAIT_OBJECT_0) {
            fprintf ( stderr, "epicsThreadSleep: WaitForSingleObject failed %lu\n", GetLastError() );
        }
    }
}

/*
 * epicsThreadSleepQuantum ()
 */
double epicsStdCall epicsThreadSleepQuantum ()
{
    /*
     * Its worth noting here that the sleep quantum on windows can
     * mysteriously get better. I eventually tracked this down to
     * codes that call timeBeginPeriod(1). Calling timeBeginPeriod()
     * specifying a better timer resolution also increases the interrupt
     * load. This appears to be related to java applet activity.
     * The function timeGetDevCaps can tell us the range of periods
     * that can be specified to timeBeginPeriod, but alas there
     * appears to be no way to find out what the value of the global
     * minimum of all timeBeginPeriod calls for all processes is.
     */
    static const double secPerTick = 100e-9;
    DWORD adjustment;
    DWORD delay;
    BOOL disabled;
    BOOL success;

    success = GetSystemTimeAdjustment (
        & adjustment, & delay, & disabled );
    if ( success ) {
        return delay * secPerTick;
    }
    else {
        return 0.0;
    }
}

/*
 * epicsThreadGetIdSelf ()
 */
LIBCOM_API epicsThreadId epicsStdCall epicsThreadGetIdSelf (void) 
{
    win32ThreadParam * pParm = getMyWin32ThreadParam ( NULL );
    assert ( pParm ); /* Don't return a NULL thread id */
    return ( epicsThreadId ) pParm;
}

LIBCOM_API epicsThreadId epicsStdCall epicsThreadGetId ( const char * pName )
{
    win32ThreadGlobal * pGbl = fetchWin32ThreadGlobal ();
    win32ThreadParam * pParm;

    if ( ! pGbl ) {
        return 0;
    }

    EnterCriticalSection ( & pGbl->mutex );

    for ( pParm = ( win32ThreadParam * ) ellFirst ( & pGbl->threadList );
            pParm; pParm = ( win32ThreadParam * ) ellNext ( & pParm->node ) ) {
        if ( pParm->pName ) {
            if ( strcmp ( pParm->pName, pName ) == 0 ) {
                break;
            }
        }
    }

    LeaveCriticalSection ( & pGbl->mutex );

    /* !!!! warning - the thread parm could vanish at any time !!!! */

    return ( epicsThreadId ) pParm;
}


/*
 * epicsThreadGetNameSelf ()
 */
LIBCOM_API const char * epicsStdCall epicsThreadGetNameSelf (void)
{
    win32ThreadParam * pParm = getMyWin32ThreadParam ( NULL );

    if ( pParm ) {
        if ( pParm->pName ) {
            return pParm->pName;
        }
    }
    return "anonymous";
}

/*
 * epicsThreadGetName ()
 */
LIBCOM_API void epicsStdCall epicsThreadGetName ( 
    epicsThreadId id, char * pName, size_t size )
{
    win32ThreadParam * pParm = ( win32ThreadParam * ) id;

    if ( size ) {
        size_t sizeMinusOne = size-1;
        strncpy ( pName, pParm->pName, sizeMinusOne );
        pName [sizeMinusOne] = '\0';
    }
}

/*
 * epics_GetThreadPriorityAsString ()
 */
static const char * epics_GetThreadPriorityAsString ( HANDLE thr )
{
    const char * pPriName = "?????";
    switch ( GetThreadPriority ( thr ) ) {
    case THREAD_PRIORITY_TIME_CRITICAL:
        pPriName = "tm-crit";
        break;
    case THREAD_PRIORITY_HIGHEST:
        pPriName = "high";
        break;
    case THREAD_PRIORITY_ABOVE_NORMAL:
        pPriName = "normal+";
        break;
    case THREAD_PRIORITY_NORMAL:
        pPriName = "normal";
        break;
    case THREAD_PRIORITY_BELOW_NORMAL:
        pPriName = "normal-";
        break;
    case THREAD_PRIORITY_LOWEST:
        pPriName = "low";
        break;
    case THREAD_PRIORITY_IDLE:
        pPriName = "idle";
        break;
    }
    return pPriName;
}

/*
 * epicsThreadShowInfo ()
 */
static void epicsThreadShowInfo ( epicsThreadId id, unsigned level )
{
    win32ThreadParam * pParm = ( win32ThreadParam * ) id;

    if ( pParm ) {
        unsigned long idForFormat = pParm->id;
        fprintf ( epicsGetStdout(), "%-15s %-8p %-8lx %-9u %-9s %-7s", pParm->pName,
            (void *) pParm, idForFormat, pParm->epicsPriority,
            epics_GetThreadPriorityAsString ( pParm->handle ),
            epicsThreadIsSuspended ( id ) ? "suspend" : "ok" );
        if ( level ) {
            fprintf (epicsGetStdout(), " %-8p %-8p ",
                (void *) pParm->handle, (void *) pParm->parm );
        }
    }
    else {
        fprintf (epicsGetStdout(),
            "NAME            EPICS-ID WIN32-ID EPICS-PRI WIN32-PRI STATE  " );
        if ( level ) {
            fprintf (epicsGetStdout(), " HANDLE   FUNCTION PARAMETER" );
        }
    }
    fprintf (epicsGetStdout(),"\n" );
}

/*
 * epicsThreadMap ()
 */
LIBCOM_API void epicsThreadMap ( EPICS_THREAD_HOOK_ROUTINE func )
{
    win32ThreadGlobal * pGbl = fetchWin32ThreadGlobal ();
    win32ThreadParam * pParm;

    if ( ! pGbl ) {
        return;
    }

    EnterCriticalSection ( & pGbl->mutex );

    for ( pParm = ( win32ThreadParam * ) ellFirst ( & pGbl->threadList );
            pParm; pParm = ( win32ThreadParam * ) ellNext ( & pParm->node ) ) {
        func ( ( epicsThreadId ) pParm );
    }

    LeaveCriticalSection ( & pGbl->mutex );
}

/*
 * epicsThreadShowAll ()
 */
LIBCOM_API void epicsStdCall epicsThreadShowAll ( unsigned level )
{
    win32ThreadGlobal * pGbl = fetchWin32ThreadGlobal ();
    win32ThreadParam * pParm;

#ifdef USE_TLSALLOC_FALLBACK
    fprintf(epicsGetStdout(), "Warning: For this target, use of epicsThread* from non-EPICS threads\n"
                              "         May leak memory.  Recommend to upgrade to >= Window Vista\n");
#endif

    if ( ! pGbl ) {
        return;
    }

    EnterCriticalSection ( & pGbl->mutex );

    epicsThreadShowInfo ( 0, level );
    for ( pParm = ( win32ThreadParam * ) ellFirst ( & pGbl->threadList );
            pParm; pParm = ( win32ThreadParam * ) ellNext ( & pParm->node ) ) {
        epicsThreadShowInfo ( ( epicsThreadId ) pParm, level );
    }

    LeaveCriticalSection ( & pGbl->mutex );
}

/*
 * epicsThreadShow ()
 */
LIBCOM_API void epicsStdCall epicsThreadShow ( epicsThreadId id, unsigned level )
{
    epicsThreadShowInfo ( 0, level );
    epicsThreadShowInfo ( id, level );
}

/*
 * epicsThreadOnce ()
 */
LIBCOM_API void epicsStdCall epicsThreadOnce (
    epicsThreadOnceId *id, void (*func)(void *), void *arg )
{
    static struct epicsThreadOSD threadOnceComplete;
    #define EPICS_THREAD_ONCE_DONE & threadOnceComplete
    win32ThreadGlobal * pGbl = fetchWin32ThreadGlobal ();

    assert ( pGbl );

    EnterCriticalSection ( & pGbl->mutex );

    if ( *id != EPICS_THREAD_ONCE_DONE ) {
        if ( *id == EPICS_THREAD_ONCE_INIT ) { /* first call */
            *id = epicsThreadGetIdSelf();      /* mark active */
            LeaveCriticalSection ( & pGbl->mutex );
            func ( arg );
            EnterCriticalSection ( & pGbl->mutex );
            *id = EPICS_THREAD_ONCE_DONE;      /* mark done */
        } else if ( *id == epicsThreadGetIdSelf() ) {
            LeaveCriticalSection ( & pGbl->mutex );
            cantProceed( "Recursive epicsThreadOnce() initialization\n" );
        } else
            while ( *id != EPICS_THREAD_ONCE_DONE ) {
                /* Another thread is in the above func(arg) call. */
                LeaveCriticalSection ( & pGbl->mutex );
                epicsThreadSleep ( epicsThreadSleepQuantum() );
                EnterCriticalSection ( & pGbl->mutex );
            }
    }
    LeaveCriticalSection ( & pGbl->mutex );
}

/*
 * epicsThreadPrivateCreate ()
 */
LIBCOM_API epicsThreadPrivateId epicsStdCall epicsThreadPrivateCreate ()
{
    epicsThreadPrivateOSD *p = ( epicsThreadPrivateOSD * ) malloc ( sizeof ( *p ) );
    if ( p ) {
        p->key = TlsAlloc ();
        if ( p->key == 0xFFFFFFFF ) {
            free ( p );
            p = 0;
        }
    }
    return p;
}

/*
 * epicsThreadPrivateDelete ()
 */
LIBCOM_API void epicsStdCall epicsThreadPrivateDelete ( epicsThreadPrivateId p )
{
    BOOL stat = TlsFree ( p->key );
    assert ( stat );
    free ( p );
}

/*
 * epicsThreadPrivateSet ()
 */
LIBCOM_API void epicsStdCall epicsThreadPrivateSet ( epicsThreadPrivateId pPvt, void *pVal )
{
    BOOL stat = TlsSetValue ( pPvt->key, (void *) pVal );
    assert (stat);
}

/*
 * epicsThreadPrivateGet ()
 */
LIBCOM_API void * epicsStdCall epicsThreadPrivateGet ( epicsThreadPrivateId pPvt )
{
    return ( void * ) TlsGetValue ( pPvt->key );
}

/*
 * epicsThreadGetCPUs ()
 */
LIBCOM_API int epicsThreadGetCPUs ( void )
{
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    if (sysinfo.dwNumberOfProcessors > 0)
        return sysinfo.dwNumberOfProcessors;
    return 1;
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
