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
    int needed = _vscprintf(fmt, ap);
    int retval = _vsnprintf(str, len, fmt, ap);

    if (len && retval == -1)
        str[len - 1] = 0;

    return ((int) len < needed) ? needed : retval;
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
