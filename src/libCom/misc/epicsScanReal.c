/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*epicsScanReal.c*/
/*Author: Eric Norum */

#include <epicsStdlib.h>

#define epicsExportSharedSymbols
#include "epicsStdio.h"


epicsShareFunc int epicsShareAPI epicsScanDouble(const char *str, double *dest)
{
    char *endp;
    double dtmp;

    dtmp = epicsStrtod(str, &endp);
    if (endp == str)
        return 0;
    *dest = dtmp;
    return 1;
}

epicsShareFunc int epicsShareAPI epicsScanFloat(const char *str, float *dest)
{
    char *endp;
    double dtmp;

    dtmp = epicsStrtod(str, &endp);
    if (endp == str)
        return 0;
    *dest = dtmp;
    return 1;
}
