/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <osiUnistd.h>
#define epicsExportSharedSymbols
#include <epicsStdio.h>

epicsShareFunc int epicsShareAPI epicsSnprintf(
    char *str, size_t size, const char *format, ...)
{
    int nchars;
    va_list pvar;

    va_start(pvar,format);
    nchars = epicsVsnprintf(str,size,format,pvar);
    va_end (pvar);
    return(nchars);
}

epicsShareFunc int epicsShareAPI epicsVsnprintf(
    char *str, size_t size, const char *format, va_list ap)
{
    return vsnprintf ( str, size, format, ap );
}
