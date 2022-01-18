/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef epicsBaseDebugLogh
#define epicsBaseDebugLogh

#include  <string.h>
#include  <epicsStdio.h>
#include  <stdarg.h>
#include  "epicsTime.h"

#ifdef __cplusplus
extern "C" {
#endif

    void epicsBaseDoDebugLog(const char *format, ...)
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
