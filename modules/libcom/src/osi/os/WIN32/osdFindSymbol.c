/*************************************************************************\
* Copyright (c) 2013 Dirk Zimoch, PSI
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* osi/os/WIN32/osdFindSymbol.c */

/* Avoid need to link against psapi.dll. requires windows 7 or later.
 * MinGW doesn't provide wrappers until 6.0, so fallback to psapi.dll
 */
#ifdef _MSC_VER
#  define NTDDI_VERSION NTDDI_WIN7
#  define epicsEnumProcessModules K32EnumProcessModules
#else
#  define epicsEnumProcessModules EnumProcessModules
#endif

#include <windows.h>
#include <psapi.h>

#define epicsExportSharedSymbols
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

epicsShareFunc void * epicsLoadLibrary(const char *name)
{
    HMODULE lib;

    epicsLoadErrorCode = 0;
    lib = LoadLibrary(name);
    if (lib == NULL)
    {
        epicsLoadErrorCode = GetLastError();
    }
    return lib;
}

epicsShareFunc const char *epicsLoadError(void)
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

epicsShareFunc void * epicsShareAPI epicsFindSymbol(const char *name)
{
    HMODULE dlls[128];
    DWORD ndlls=0u, i;
    void* ret = NULL;

    /* As a handle returned by LoadLibrary() isn't available to us,
     * try all loaded modules in arbitrary order.
     */
    if(epicsEnumProcessModules(GetCurrentProcess(), dlls, sizeof(dlls), &ndlls)) {
        for(i=0; !ret && i<ndlls; i++) {
            ret = GetProcAddress(dlls[i], name);
        }
    }
    if(!ret) {
        /* only capturing the last error code,
         * but what else to do?
         */
        epicsLoadErrorCode = GetLastError();
    }
    return ret;
}
