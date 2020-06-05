/*************************************************************************\
* Copyright (c) 2013 Dirk Zimoch, PSI
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* osi/os/WIN32/osdFindSymbol.c */

/* Avoid need to link against psapi.dll. requires windows 7 or later.
 * MinGW doesn't provide wrappers until 6.0, so fallback to psapi.dll
 */
#ifdef _MSC_VER
#  define PSAPI_VERSION 2
#  define epicsEnumProcessModules K32EnumProcessModules
#else
#  define epicsEnumProcessModules EnumProcessModules
#endif

#include <windows.h>
#include <psapi.h>

#include "epicsStdio.h"
#include "epicsFindSymbol.h"

#ifdef _MSC_VER
#  define STORE static __declspec( thread )
#elif __GNUC__
#  define STORE static __thread
#else
#  define STORE static
#endif

STORE
DWORD epicsLoadErrorCode = 0;

LIBCOM_API void * epicsLoadLibrary(const char *name)
{
    HMODULE lib;

    epicsLoadErrorCode = 0;
    lib = LoadLibrary(name);
    epicsLoadErrorCode = lib ? 0 : GetLastError();
    return lib;
}

LIBCOM_API const char *epicsLoadError(void)
{
    STORE char buffer[100];
    DWORD n;

    n = FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        epicsLoadErrorCode,
        0,
        buffer,
        sizeof(buffer)-1, NULL );

    if(n) {
        /* n - number of chars stored excluding nil.
         *
         * Some messages include a trailing newline, which we strip.
         */
        for(; n>=1 && (buffer[n-1]=='\n' || buffer[n-1]=='\r'); n--)
            buffer[n-1] = '\0';
    } else {
         epicsSnprintf(buffer, sizeof(buffer),
                       "Unable to format WIN32 error code %lu",
                       (unsigned long)epicsLoadErrorCode);
         buffer[sizeof(buffer)-1] = '\0';

    }

    return buffer;
}

LIBCOM_API void * epicsStdCall epicsFindSymbol(const char *name)
{
    HANDLE proc = GetCurrentProcess();
    HMODULE *dlls=NULL;
    DWORD nalloc=0u, needed=0u;
    void* ret = NULL;

    /* As a handle returned by LoadLibrary() isn't available to us,
     * try all loaded modules in arbitrary order.
     */

    if(!epicsEnumProcessModules(proc, NULL, 0, &needed)) {
        epicsLoadErrorCode = GetLastError();
        return ret;
    }

    if(!(dlls = malloc(nalloc = needed))) {
        epicsLoadErrorCode = ERROR_NOT_ENOUGH_MEMORY;
        return ret;
    }

    if(epicsEnumProcessModules(proc, dlls, nalloc, &needed)) {
        DWORD i, ndlls;

        /* settle potential races w/o retry by iterating smaller of nalloc or needed */
        if(nalloc > needed)
            nalloc = needed;

        for(i=0, ndlls = nalloc/sizeof(*dlls); !ret && i<ndlls; i++) {
            ret = GetProcAddress(dlls[i], name);
            if(!ret && GetLastError()!=ERROR_PROC_NOT_FOUND) {
                break;
            }
        }
    }

    epicsLoadErrorCode = ret ? 0 : GetLastError();
    free(dlls);
    return ret;
}
