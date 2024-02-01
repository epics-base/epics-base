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

LIBCOM_API const char * epicsStdCall epicsBaseDebugStripPath(const char *file);
LIBCOM_API void epicsStdCall epicsNetDoDebugLog(const char *, ...) EPICS_PRINTF_STYLE(1,2);

#define epicsNetDebugLog(fmt, ...)                                  \
    {                                                                \
        epicsTimeStamp now;                                          \
        char nowText[25];                                            \
        size_t rtn;                                                  \
        nowText[0] = 0;                                              \
        rtn = epicsTimeGetCurrent(&now);                             \
        if (!rtn) {                                                  \
            epicsTimeToStrftime(nowText,sizeof(nowText),             \
                                "%Y/%m/%d %H:%M:%S.%03f",&now);      \
        }                                                            \
        epicsNetDoDebugLog("%s NET %s:%-4d " fmt,                    \
                            nowText,                                 \
                            epicsBaseDebugStripPath(__FILE__), __LINE__, \
                            __VA_ARGS__);                            \
    }

#define epicsNetDebugLogFL(fmt, fileName, lineNo, ...)              \
    {                                                                \
        epicsTimeStamp now;                                          \
        char nowText[25];                                            \
        size_t rtn;                                                  \
        nowText[0] = 0;                                              \
        rtn = epicsTimeGetCurrent(&now);                             \
        if (!rtn) {                                                  \
            epicsTimeToStrftime(nowText,sizeof(nowText),             \
                                "%Y/%m/%d %H:%M:%S.%03f",&now);      \
        }                                                            \
        epicsNetDoDebugLog("%s NET " fmt,                            \
                            nowText,                                 \
                            epicsBaseDebugStripPath(fileName), lineNo,  \
                            __VA_ARGS__);                            \
    }

#ifdef __cplusplus
}
#endif

#endif
