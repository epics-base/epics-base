
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
#include "ellLib.h"

typedef struct win32ThreadParam {
    ELLNODE node;
    HANDLE handle;
    EPICSTHREADFUNC funptr;
    void *parm;
    char *pName;
    DWORD id;
    char isSuspended;
} win32ThreadParam;

static ELLLIST threadList;

static DWORD tlsIndexThreadLibraryEPICS = 0xFFFFFFFF;

typedef struct epicsThreadPrivateOSD {
    DWORD key;
} epicsThreadPrivateOSD;

static HANDLE win32ThreadGlobalMutex = 0;
static int win32ThreadInitOK = 0;

#if WINVER >= 500 && 0
#   define osdPriorityStateCount 14u
#else
#   define osdPriorityStateCount 5u
#endif
//
// apparently these additional priorities only work if the process 
// priority class is real time?
//
static const int osdPriorityList [osdPriorityStateCount] = 
{
#if WINVER >= 500 && 0
    -7, // allowed on >= W2k, but no #define supplied
    -6, // allowed on >= W2k, but no #define supplied
    -5, // allowed on >= W2k, but no #define supplied
    -4, // allowed on >= W2k, but no #define supplied
    -3,  // allowed on >= W2k, but no #define supplied
#endif
    THREAD_PRIORITY_LOWEST,       // -2 on >= W2K ??? on W95
    THREAD_PRIORITY_BELOW_NORMAL, // -1 on >= W2K ??? on W95
    THREAD_PRIORITY_NORMAL,       //  0 on >= W2K ??? on W95
    THREAD_PRIORITY_ABOVE_NORMAL, //  1 on >= W2K ??? on W95
    THREAD_PRIORITY_HIGHEST       //  2 on >= W2K ??? on W95
#if WINVER >= 500 && 0
    3, // allowed on >= W2k, but no #define supplied
    4, // allowed on >= W2k, but no #define supplied
    5, // allowed on >= W2k, but no #define supplied
    6  // allowed on >= W2k, but no #define supplied
#endif
};

static void epicsParmCleanupWIN32 ( win32ThreadParam * pParm )
{
    BOOL success;
    DWORD stat;

    if ( pParm ) {
        stat = WaitForSingleObject ( win32ThreadGlobalMutex, INFINITE );
        assert ( stat == WAIT_OBJECT_0 );
    
        ellDelete ( & threadList, & pParm->node );

        success = ReleaseMutex ( win32ThreadGlobalMutex );
        assert ( success );

        // close the handle if its an implicit thread id
        if ( ! pParm->funptr ) {
            CloseHandle ( pParm->handle );
        }
        free ( pParm );
        TlsSetValue ( tlsIndexThreadLibraryEPICS, 0 );
    }
}

/*
 * threadCleanupWIN32 ()
 */
static void threadCleanupWIN32 ( void )
{
    win32ThreadParam * pParm;

    WaitForSingleObject ( win32ThreadGlobalMutex, INFINITE );
    while ( pParm = ( win32ThreadParam * ) ellFirst ( & threadList ) ) {
        epicsParmCleanupWIN32 ( pParm );
    }
    ReleaseMutex ( win32ThreadGlobalMutex );

    if ( tlsIndexThreadLibraryEPICS != 0xFFFFFFFF ) {
        TlsFree ( tlsIndexThreadLibraryEPICS );
        tlsIndexThreadLibraryEPICS = 0xFFFFFFFF;
    }

    if ( win32ThreadGlobalMutex ) {
        CloseHandle ( win32ThreadGlobalMutex );
        win32ThreadGlobalMutex = NULL;
    }
}

/*
 * epicsThreadExitMain ()
 */
epicsShareFunc void epicsShareAPI epicsThreadExitMain ( void )
{
    _endthread ();
}

/*
 * osdPriorityMagFromPriorityOSI ()
 */
static unsigned osdPriorityMagFromPriorityOSI ( unsigned osiPriority ) 
{
    unsigned magnitude;

    // optimizer will remove this one if epicsThreadPriorityMin is zero
    // and osiPriority is unsigned
    if ( osiPriority < epicsThreadPriorityMin ) {
        osiPriority = epicsThreadPriorityMin;
    }

    if ( osiPriority > epicsThreadPriorityMax ) {
        osiPriority = epicsThreadPriorityMax;
    }

    magnitude = osiPriority * osdPriorityStateCount;
    magnitude /= ( epicsThreadPriorityMax - epicsThreadPriorityMin ) + 1;

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
    unsigned magnitude = osdPriorityMagFromPriorityOSD ( osdPriority );
    return osiPriorityMagFromMagnitueOSD (magnitude);
}

/*
 * epicsThreadLowestPriorityLevelAbove ()
 */
epicsShareFunc epicsThreadBooleanStatus epicsShareAPI epicsThreadLowestPriorityLevelAbove 
            ( unsigned int priority, unsigned *pPriorityJustAbove )
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
            ( unsigned int priority, unsigned *pPriorityJustBelow )
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
epicsShareFunc unsigned int epicsShareAPI epicsThreadGetStackSize ( epicsThreadStackSizeClass stackSizeClass ) 
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

void epicsThreadCleanupWIN32 ()
{
    win32ThreadParam * pParm = (win32ThreadParam *) TlsGetValue ( tlsIndexThreadLibraryEPICS );
    epicsParmCleanupWIN32 ( pParm );
}

/*
 * this voodo copied directly from example in visual c++ 7 documentation
 *
 * Usage: SetThreadName (-1, "MainThread");
 */
void SetThreadName( DWORD dwThreadID, LPCSTR szThreadName )
{
    typedef struct tagTHREADNAME_INFO
    {
        DWORD dwType; // must be 0x1000
        LPCSTR szName; // pointer to name (in user addr space)
        DWORD dwThreadID; // thread ID (-1=caller thread)
        DWORD dwFlags; // reserved for future use, must be zero
    } THREADNAME_INFO;
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = szThreadName;
    info.dwThreadID = dwThreadID;
    info.dwFlags = 0;

    __try
    {
        RaiseException ( 0x406D1388, 0, 
            sizeof(info)/sizeof(DWORD), (DWORD*)&info );
    }
    __except ( EXCEPTION_CONTINUE_EXECUTION )
    {
    }
}

/*
 * epicsWin32ThreadEntry()
 */
static unsigned WINAPI epicsWin32ThreadEntry ( LPVOID lpParameter )
{
    win32ThreadParam *pParm = (win32ThreadParam *) lpParameter;
    BOOL success;

    SetThreadName ( pParm->id, pParm->pName );

    success = TlsSetValue ( tlsIndexThreadLibraryEPICS, pParm );
    if ( success ) {
        /* printf ( "starting thread %d\n", pParm->id ); */
        ( *pParm->funptr ) ( pParm->parm );
        /* printf ( "terminating thread %d\n", pParm->id ); */
    }

    /*
     * CAUTION: !!!! the thread id might continue to be used after this thread exits !!!!
     */
    epicsParmCleanupWIN32 ( pParm );

    return ( ( unsigned ) success ); /* this indirectly closes the thread handle */
}

/*
 * epicsThreadInit ()
 */
static void epicsThreadInit ( void )
{
    HANDLE win32ThreadGlobalMutexTmp;
    DWORD status;
    BOOL success;
    int crtlStatus;

    if ( win32ThreadGlobalMutex ) {
        /* wait for init to complete */
        status = WaitForSingleObject ( win32ThreadGlobalMutex, INFINITE );
        assert ( status == WAIT_OBJECT_0 );
        success = ReleaseMutex ( win32ThreadGlobalMutex );
        assert ( success );
        return;
    }
    else {
        win32ThreadGlobalMutexTmp = CreateMutex ( NULL, TRUE, NULL );
        if ( win32ThreadGlobalMutexTmp == 0 ) {
            return;
        }

#if 1
        /* not arch neutral, but at least supported by w95 and borland */
        if ( InterlockedExchange ( (LPLONG) &win32ThreadGlobalMutex, (LONG) win32ThreadGlobalMutexTmp ) ) {
#else
        /* not supported on W95, but the alternative requires assuming that pointer and integer are the same */
        if ( InterlockedCompareExchange ( (PVOID *) &win32ThreadGlobalMutex, 
            (PVOID) win32ThreadGlobalMutexTmp, (PVOID)0 ) != 0) {
#endif
            CloseHandle (win32ThreadGlobalMutexTmp);
            /* wait for init to complete */
            status = WaitForSingleObject ( win32ThreadGlobalMutex, INFINITE );
            assert ( status == WAIT_OBJECT_0 );
            success = ReleaseMutex ( win32ThreadGlobalMutex );
            assert ( success );
            return;
        }
    }

    if ( tlsIndexThreadLibraryEPICS == 0xFFFFFFFF ) {
        tlsIndexThreadLibraryEPICS = TlsAlloc();
        if ( tlsIndexThreadLibraryEPICS == 0xFFFFFFFF ) {
            success = ReleaseMutex (win32ThreadGlobalMutex);
            assert (success);
            return;
        }
    }

    crtlStatus = atexit ( threadCleanupWIN32 );
    if (crtlStatus) {
        success = ReleaseMutex ( win32ThreadGlobalMutex );
        assert ( success );
        return;
    }

    ellInit ( & threadList );

    win32ThreadInitOK = TRUE;

    success = ReleaseMutex ( win32ThreadGlobalMutex );
    assert ( success );
}

static win32ThreadParam * epicsThreadParmCreate ( const char *pName )
{
    win32ThreadParam *pParmWIN32;

    pParmWIN32 = calloc ( 1, sizeof ( *pParmWIN32 ) + strlen ( pName ) + 1 );
    if ( pParmWIN32  ) {
        if ( pName ) {
            pParmWIN32->pName = (char *) ( pParmWIN32 + 1 );
            strcpy ( pParmWIN32->pName, pName );
        }
        else {
            pParmWIN32->pName = 0;
        }
        pParmWIN32->isSuspended = 0;
    }
    return pParmWIN32;
}

static win32ThreadParam * epicsThreadImplicitCreate ()
{
    win32ThreadParam *pParm;
    char name[64];
    DWORD id = GetCurrentThreadId ();
    HANDLE handle;
    DWORD stat;
    BOOL success;

    if ( ! win32ThreadInitOK ) {
        epicsThreadInit ();
        if ( ! win32ThreadInitOK ) {
            return NULL;
        }
    }
    success = DuplicateHandle ( GetCurrentProcess (), GetCurrentThread (),
            GetCurrentProcess (), & handle, 0, FALSE, DUPLICATE_SAME_ACCESS );
    if ( ! success ) {
        return 0;
    }
    sprintf ( name, "Implicit id=%x", id );
    pParm = epicsThreadParmCreate ( name );
    if ( pParm ) {
        pParm->handle = handle;
        pParm->id = id;
        success = TlsSetValue ( tlsIndexThreadLibraryEPICS, pParm );
        if ( ! success ) {
            epicsParmCleanupWIN32 ( pParm );
            pParm = 0;
        }
        else {
            stat = WaitForSingleObject ( win32ThreadGlobalMutex, INFINITE );
            assert ( stat == WAIT_OBJECT_0 );

            ellAdd ( & threadList, & pParm->node );

            success = ReleaseMutex ( win32ThreadGlobalMutex );
            assert ( success );
        }
    }
    return pParm;
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

    pParmWIN32 = epicsThreadParmCreate ( pName );
    if ( pParmWIN32 == 0 ) {
        return ( epicsThreadId ) pParmWIN32;
    }
    pParmWIN32->funptr = pFunc;
    pParmWIN32->parm = pParm;

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

    wstat = WaitForSingleObject ( win32ThreadGlobalMutex, INFINITE );
    assert ( wstat == WAIT_OBJECT_0 );

    ellAdd ( & threadList, & pParmWIN32->node );

    bstat = ReleaseMutex ( win32ThreadGlobalMutex );
    assert ( bstat );

    return ( epicsThreadId ) pParmWIN32;
}

/*
 * epicsThreadSuspendSelf ()
 */
epicsShareFunc void epicsShareAPI epicsThreadSuspendSelf ()
{
    win32ThreadParam *pParm = (win32ThreadParam *) TlsGetValue (tlsIndexThreadLibraryEPICS);
    BOOL success;
    DWORD stat;

    if ( ! pParm ) {
        pParm = epicsThreadImplicitCreate ();
        if ( ! pParm ) {
            stat =  SuspendThread ( GetCurrentThread () );
            assert ( stat != 0xFFFFFFFF );
            return;
        }
    }

    stat = WaitForSingleObject (win32ThreadGlobalMutex, INFINITE);
    assert ( stat == WAIT_OBJECT_0 );

    stat =  SuspendThread ( pParm->handle );
    pParm->isSuspended = 1;

    success = ReleaseMutex (win32ThreadGlobalMutex);
    assert (success);

    assert (stat!=0xFFFFFFFF);
}

/*
 * epicsThreadResume ()
 */
epicsShareFunc void epicsShareAPI epicsThreadResume ( epicsThreadId id )
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

    win32ThreadPriority = GetThreadPriority ( pParm->handle );
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
    assert ( win32ThreadPriority != THREAD_PRIORITY_ERROR_RETURN );
 
    return epicsThreadGetOsiPriorityValue ( win32ThreadPriority );
}

/*
 * epicsThreadSetPriority ()
 */
epicsShareFunc void epicsShareAPI epicsThreadSetPriority ( epicsThreadId id, unsigned priority ) 
{
    win32ThreadParam *pParm = (win32ThreadParam *) id;
    BOOL stat;

    stat = SetThreadPriority ( pParm->handle, epicsThreadGetOsdPriorityValue (priority) );
    assert (stat);
}

/*
 * epicsThreadIsEqual ()
 */
epicsShareFunc int epicsShareAPI epicsThreadIsEqual ( epicsThreadId id1, epicsThreadId id2 ) 
{
    win32ThreadParam *pParm1 = (win32ThreadParam *) id1;
    win32ThreadParam *pParm2 = (win32ThreadParam *) id2;
    return ( id1 == id2 && pParm1->id == pParm2->id );
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
epicsShareFunc int epicsShareAPI epicsThreadIsSuspended ( epicsThreadId id ) 
{
    win32ThreadParam *pParm = (win32ThreadParam *) id;
    DWORD exitCode;
    BOOL stat;
    
    stat = GetExitCodeThread ( pParm->handle, &exitCode );
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
epicsShareFunc void epicsShareAPI epicsThreadSleep ( double seconds )
{
    static const unsigned mSecPerSec = 1000;
    DWORD milliSecDelay;

    if ( seconds <= 0.0 ) {
        milliSecDelay = 0u;
    }
    else if ( seconds >= INFINITE / mSecPerSec ) {
        milliSecDelay = INFINITE - 1;
    }
    else {
        milliSecDelay = ( DWORD ) ( ( seconds * mSecPerSec ) + 0.5 );
        if ( milliSecDelay == 0 ) {
            milliSecDelay = 1;
        }
    }
    Sleep ( milliSecDelay );
}

/*
 * epicsThreadGetIdSelf ()
 */
epicsShareFunc epicsThreadId epicsShareAPI epicsThreadGetIdSelf (void) 
{
    win32ThreadParam *pParm = (win32ThreadParam *) TlsGetValue (tlsIndexThreadLibraryEPICS);
    if ( ! pParm ) {
        pParm = epicsThreadImplicitCreate ();
        assert ( pParm ); /* very dangerous to allow non-unique thread id into use */
    }
    return ( epicsThreadId ) pParm;
}

epicsShareFunc epicsThreadId epicsShareAPI epicsThreadGetId ( const char *pName )
{
    win32ThreadParam * pParm;
    DWORD stat;
    BOOL success;

    stat = WaitForSingleObject ( win32ThreadGlobalMutex, INFINITE );
    assert ( stat == WAIT_OBJECT_0 );
    
    for ( pParm = ( win32ThreadParam * ) ellFirst ( & threadList ); 
            pParm; pParm = ( win32ThreadParam * ) ellNext ( & pParm->node ) ) {
        if ( pParm->pName ) {
            if ( strcmp ( pParm->pName, pName ) == 0 ) {
                break;
            }
        }
    }

    success = ReleaseMutex ( win32ThreadGlobalMutex );
    assert ( success );

    // !!!! warning - the thread parm could vanish at any time !!!!

    return ( epicsThreadId ) pParm;
}


/*
 * epicsThreadGetNameSelf ()
 */
epicsShareFunc const char * epicsShareAPI epicsThreadGetNameSelf (void)
{
    win32ThreadParam *pParm = (win32ThreadParam *) TlsGetValue (tlsIndexThreadLibraryEPICS);
    if ( ! pParm ) {
        pParm = epicsThreadImplicitCreate ();
    }

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
epicsShareFunc void epicsShareAPI epicsThreadGetName ( epicsThreadId id, char *pName, size_t size )
{
    win32ThreadParam *pParm = (win32ThreadParam *) id;

    if ( size ) {
        size_t sizeMinusOne = size-1;
        strncpy (pName, pParm->pName, sizeMinusOne);
        pName [sizeMinusOne] = '\0';
    }
}

/*
 * epicsThreadShowAll ()
 */
epicsShareFunc void epicsShareAPI epicsThreadShowAll ( unsigned level )
{
    win32ThreadParam * pParm;
    DWORD stat;
    BOOL success;

    stat = WaitForSingleObject ( win32ThreadGlobalMutex, INFINITE );
    assert ( stat == WAIT_OBJECT_0 );
    
    printf ( "EPICS WIN32 Thread List\n" );
    for ( pParm = ( win32ThreadParam * ) ellFirst ( & threadList ); 
            pParm; pParm = ( win32ThreadParam * ) ellNext ( & pParm->node ) ) {
        epicsThreadShow ( ( epicsThreadId ) pParm, level );
    }

    success = ReleaseMutex ( win32ThreadGlobalMutex );
    assert ( success );
}

/*
 * epicsThreadShow ()
 */
epicsShareFunc void epicsShareAPI epicsThreadShow ( epicsThreadId id, unsigned level )
{
    win32ThreadParam *pParm = (win32ThreadParam *) id;

    printf ( "\"%s\" %s", pParm->pName, pParm->isSuspended?"suspended":"running");
    if ( level ) {
        printf ( " HANDLE=%p func=%p parm=%p id=%d ",
            pParm->handle, pParm->funptr, pParm->parm, pParm->id );
    }
    printf ("\n" );
}

/*
 * epicsThreadOnce ()
 */
epicsShareFunc void epicsShareAPI epicsThreadOnceOsd (
    epicsThreadOnceId *id, void (*func)(void *), void *arg )
{
    BOOL success;
    DWORD stat;
    
    if ( ! win32ThreadInitOK ) {
        epicsThreadInit ();
        assert ( win32ThreadInitOK );
    }

    stat = WaitForSingleObject ( win32ThreadGlobalMutex, INFINITE );
    assert ( stat == WAIT_OBJECT_0 );

    if ( ! *id ) {
        ( *func ) ( arg );
        *id = 1;
    }

    success = ReleaseMutex ( win32ThreadGlobalMutex );
    assert ( success );
}

/*
 * epicsThreadPrivateCreate ()
 */
epicsShareFunc epicsThreadPrivateId epicsShareAPI epicsThreadPrivateCreate ()
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
epicsShareFunc void epicsShareAPI epicsThreadPrivateDelete ( epicsThreadPrivateId p )
{
    BOOL stat = TlsFree ( p->key );
    assert ( stat );
    free ( p );
}

/*
 * epicsThreadPrivateSet ()
 */
epicsShareFunc void epicsShareAPI epicsThreadPrivateSet ( epicsThreadPrivateId pPvt, void *pVal )
{
    BOOL stat = TlsSetValue ( pPvt->key, (void *) pVal );
    assert (stat);
}

/*
 * epicsThreadPrivateGet ()
 */
epicsShareFunc void * epicsShareAPI epicsThreadPrivateGet ( epicsThreadPrivateId pPvt )
{
    return ( void * ) TlsGetValue ( pPvt->key );
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
