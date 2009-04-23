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
    float rtnvalue;

    if (value == 0) {
        rtnvalue = 0;
    } else if (!finite(value)) {
        rtnvalue = (float)value;
    } else {
        double abs = fabs(value);

        if (abs >= FLT_MAX) {
            if (value > 0)
                rtnvalue = FLT_MAX;
            else
                rtnvalue = -FLT_MAX;
        } else if (abs <= FLT_MIN) {
            if (value > 0)
                rtnvalue = FLT_MIN;
            else
                rtnvalue = -FLT_MIN;
        } else {
            rtnvalue = (float)value;
        }
    }
    return rtnvalue;
}
