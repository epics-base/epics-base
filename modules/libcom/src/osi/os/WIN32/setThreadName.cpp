
/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
#define VC_EXTRALEAN
#define STRICT
#include <windows.h>

#include <string>

/*
 * Usage: setThreadName (-1, "MainThread");
 */


static void setThreadNameVS ( DWORD dwThreadID, LPCSTR szThreadName );
typedef HRESULT (WINAPI* setDesc_t)(HANDLE, PCWSTR);

extern "C" void setThreadName ( DWORD dwThreadID, LPCSTR szThreadName )
{
    static HMODULE hKernel = LoadLibrary("KernelBase.dll");
    static setDesc_t pSetDesc = (hKernel != NULL ?
        (setDesc_t)GetProcAddress(hKernel, "SetThreadDescription") : NULL);
    if (szThreadName == NULL || *szThreadName == '\0')
    {
        return;
    }
    if (pSetDesc != NULL)
    {
#ifdef THREAD_SET_LIMITED_INFORMATION
        DWORD thread_access = THREAD_SET_LIMITED_INFORMATION;
#else
        DWORD thread_access = THREAD_SET_INFORMATION;
#endif /* ifdef THREAD_SET_LIMITED_INFORMATION */
        HANDLE hThread = OpenThread(thread_access, FALSE, dwThreadID);
        if (hThread != NULL)
        {
            const std::string s(szThreadName);
            const std::wstring ws(s.begin(), s.end());
            HRESULT hr = (*pSetDesc)(hThread, ws.c_str());
            CloseHandle(hThread);
        }
    }
    // if SetThreadDescription() was available and we have a recent
    // visual studio debugger (2017 version 15.6 or higher) attached
    // then the names will already be defined. However we don't know
    // this for sure, so also trigger the old exception mechanism.
    // See https://docs.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code
    setThreadNameVS(dwThreadID, szThreadName);
}

static void setThreadNameVS( DWORD dwThreadID, LPCSTR szThreadName )
{
#if _MSC_VER >= 1300 && defined ( _DEBUG )
// This was copied directly from an MSDN example
// It sets the thread name by throwing a special exception that is caught by Visual Sudio
// It requires the debugger to be already attached to the process 
// when the exception is thrown for the name to be registered
    static const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push,8)
    typedef struct tagTHREADNAME_INFO
    {
        DWORD dwType;     // Must be 0x1000.
        LPCSTR szName;    // Pointer to name (in user addr space).
        DWORD dwThreadID; // Thread ID (-1=caller thread).
        DWORD dwFlags;    // Reserved for future use, must be zero.
    } THREADNAME_INFO;
#pragma pack(pop)
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = szThreadName;
    info.dwThreadID = dwThreadID;
    info.dwFlags = 0;
#pragma warning(push)
#pragma warning(disable: 6320 6322)
    __try
    {
        RaiseException(MS_VC_EXCEPTION, 0,
                       sizeof(info) / sizeof(ULONG_PTR),
                       (ULONG_PTR*)&info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
#pragma warning(pop)
#endif
}
