/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <osiUnistd.h>
#include <epicsStdio.h>

LIBCOM_API int epicsStdCall epicsSnprintf(
    char *str, size_t size, const char *format, ...)
{
    int nchars;
    va_list pvar;

    va_start(pvar,format);
    nchars = epicsVsnprintf(str,size,format,pvar);
    va_end (pvar);
    return(nchars);
}

LIBCOM_API int epicsStdCall epicsVsnprintf(
    char *str, size_t size, const char *format, va_list ap)
{
    return vsnprintf ( str, size, format, ap );
}
