/* osdStdio.c */
/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <stdio.h>
#include <stdarg.h>

#define epicsExportSharedSymbols
#include "epicsStdio.h"

int epicsShareAPI epicsVsnprintf (
    char * str, size_t size, const char *format, va_list ap )
{
    int rtn;

    rtn = _vsnprintf ( str, size, format, ap );
    if ( rtn >= 0 ) {
        size_t sizeReturned = (size_t) rtn;
        if ( sizeReturned < size ) {
            return rtn;
        }
    }
    if ( rtn == -1 ) {
        if(size!=0) str[size-1] = 0;
        return (int) size;
    }
    return rtn;
}

int epicsShareAPI epicsSnprintf (
    char *str, size_t size, const char *pFormat, ... )
{
    int rtn;
    va_list pvar;

    va_start ( pvar, pFormat );
    rtn = epicsVsnprintf ( str, size, pFormat, pvar );
    va_end ( pvar );
    return ( rtn );
}
