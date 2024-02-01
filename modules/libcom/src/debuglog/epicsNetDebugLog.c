/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef epicsNetDebugLogh
#define epicsNetDebugLogh

#include  <string.h>
#include  <epicsStdio.h>
#include  <stdarg.h>
#include  "epicsTime.h"

#ifdef __cplusplus
extern "C" {
#endif

LIBCOM_API const char * epicsStdCall epicsBaseDebugStripPath(const char *file)
    {
        const char *ret = strrchr(file, '/');
        if (ret) return ret + 1;
#if (defined(CYGWIN32) || defined(_WIN32))
        ret = strrchr(file, '\\');
        if (ret) return ret + 1;
#endif
        return file;
    }


LIBCOM_API void epicsStdCall epicsNetDoDebugLog(const char *format, ...)
    {
        va_list pVar;
        va_start(pVar, format);
        vfprintf(stdout, format, pVar);
        va_end(pVar);
        fflush(stdout);
    }

#ifdef __cplusplus
}
#endif

#endif
