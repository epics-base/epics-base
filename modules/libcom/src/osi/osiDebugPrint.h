/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef osiDebugPrinth
#define osiDebugPrinth

#include  <string.h>
#include  <epicsStdio.h>
#include  <stdarg.h>
#include  "epicsTime.h"


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
  static inline void osiDebugDoPrint(const char *, ...) EPICS_PRINTF_STYLE(1,2);
  static inline void osiDebugDoPrint(const char *format, ...)
  {
    va_list pVar;
    va_start(pVar, format);
    vfprintf(stdout, format, pVar);
    va_end(pVar);
    fflush(stdout);
  }

#define osiDebugPrint(fmt, ...)                                      \
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
        osiDebugDoPrint("%s %s:%-4d " fmt,                           \
                        nowText,                                     \
                        osiDebugStripPath(__FILE__), __LINE__,       \
                        __VA_ARGS__);                                \
    }


#endif
