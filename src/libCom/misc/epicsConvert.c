/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*epicsConvert.c*/

#include <float.h>
#include <limits.h>

#define epicsExportSharedSymbols
#include "epicsMath.h"
#include "epicsConvert.h"
#include "cantProceed.h"

epicsShareFunc float epicsConvertDoubleToFloat(double value)
{
    double abs;

    if (value == 0 || !finite(value))
        return (float) value;

    abs = fabs(value);

    if (abs >= FLT_MAX)
        return (value > 0) ? FLT_MAX : -FLT_MAX;

    if (abs <= FLT_MIN)
        return (value > 0) ? FLT_MIN : -FLT_MIN;

    return (float) value;
}
