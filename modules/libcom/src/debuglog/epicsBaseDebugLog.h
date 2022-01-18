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

    static inline const char *osiDebugStripPath(const char *file)
    {
        const char *ret = strrchr(file, '/');
        if (ret) return ret + 1;
#if (defined(CYGWIN32) || defined(_WIN32))
        ret = strrchr(file, '\\');
        if (ret) return ret + 1;
#endif
        return file;
    }

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
                        nowText,                                     \
                        osiDebugStripPath(__FILE__), __LINE__,       \
                        __VA_ARGS__);                                \
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
                            osiDebugStripPath(fileName), lineNo,     \
                            __VA_ARGS__);                            \
    }

#ifdef __cplusplus
}
#endif

#endif
