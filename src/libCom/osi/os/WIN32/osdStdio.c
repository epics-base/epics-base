/* osdStdio.c */
/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <stdio.h>
#include <stdarg.h>

#define epicsExportSharedSymbols
#include "epicsStdio.h"

int epicsShareAPI epicsVsnprintf(char *str, size_t len,
    const char *fmt, va_list ap)
{
    int retval = _vsnprintf(str, len, fmt, ap);

#ifdef _MSC_VER
    int needed = _vscprintf(fmt, ap);

    if ((int) len < needed + 1) {
        str[len - 1] = 0;
        return needed;
    }
#else
    /* Unfortunately MINGW doesn't provide _vscprintf and their
     * _vsnprintf follows MS' broken return value semantics.
     */
    if (retval == -1) {
        if (len)
            str[len - 1] = 0;
        return len;
    } else if (retval == (int) len) {
        str[--retval] = 0;
    }
#endif

    return retval;
}

int epicsShareAPI epicsSnprintf (char *str, size_t len, const char *fmt, ...)
{
    int rtn;
    va_list pvar;

    va_start (pvar, fmt);
    rtn = epicsVsnprintf (str, len, fmt, pvar);
    va_end (pvar);
    return (rtn);
}
