
/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#define VC_EXTRALEAN
#define STRICT
#if _WIN64
#   define _WIN32_WINNT 0x400 /* defining this drops support for W95 */
#endif
#include <windows.h>

/*
 * this was copied directly from an example in visual c++ 7 documentation,
 * It uses visual C++ specific keywords for exception handling, but is
 * probably only useful when using the visual c++ or later debugger.
 *
 * Usage: setThreadName (-1, "MainThread");
 */
extern "C" void setThreadName ( DWORD dwThreadID, LPCSTR szThreadName )
{
#if _MSC_VER >= 1300 && defined ( _DEBUG )
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
        RaiseException( 0x406D1388, 0, 
            sizeof(info)/sizeof(DWORD), (const ULONG_PTR*)&info );
    }
    __except(EXCEPTION_CONTINUE_EXECUTION)
    {
    }
#endif
}
