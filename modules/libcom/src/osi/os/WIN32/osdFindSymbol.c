/*************************************************************************\
* Copyright (c) 2013 Dirk Zimoch, PSI
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* osi/os/WIN32/osdFindSymbol.c */


#include <windows.h>

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
int epicsLoadErrorCode = 0;

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

    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        epicsLoadErrorCode,
        0,
        buffer,
        sizeof(buffer)-1, NULL );
    return buffer;
}

epicsShareFunc void * epicsShareAPI epicsFindSymbol(const char *name)
{
    void* ret = GetProcAddress(0, name);
    if(!ret) {
        epicsLoadErrorCode = GetLastError();
    }
    return ret;
}
