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

    void epicsBaseDoDebugLog(const char *, ...) EPICS_PRINTF_STYLE(1,2);
    void epicsBaseDoDebugLog(const char *format, ...);

    const char *epicsBaseDebugStripPath(const char *file);


#define epicsBaseDebugLog(fmt, ...)                                  \
    {                                                                \
        epicsTimeStamp now;                                          \
        char nowText[25];                                            \
        size_t rtn;                                                  \
        nowText[0] = 0;                                              \
        rtn = epicsTimeGetCurrent(&now);                             \
        if (!rtn) {                                                  \
            epicsTimeToStrftime(nowText,sizeof(nowText),             \
                                "%Y/%m/%d %H:%M:%S.%03f ",&now);     \
        }                                                            \
        epicsBaseDoDebugLog("%s %s:%-4d " fmt,                       \
                            nowText,                                 \
                            epicsBaseDebugStripPath(__FILE__), __LINE__, \
                            __VA_ARGS__);                            \
    }

#define epicsBaseDebugLogFL(fmt, fileName, lineNo, ...)              \
    {                                                                \
        epicsTimeStamp now;                                          \
        char nowText[25];                                            \
        size_t rtn;                                                  \
        nowText[0] = 0;                                              \
        rtn = epicsTimeGetCurrent(&now);                             \
        if (!rtn) {                                                  \
            epicsTimeToStrftime(nowText,sizeof(nowText),             \
                                "%Y/%m/%d %H:%M:%S.%03f ",&now);     \
        }                                                            \
        epicsBaseDoDebugLog("%s " fmt,                               \
                            nowText,                                 \
                            epicsBaseDebugStripPath(fileName), lineNo,  \
                            __VA_ARGS__);                            \
    }

#ifdef __cplusplus
}
#endif

#endif
