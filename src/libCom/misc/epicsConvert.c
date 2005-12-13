/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*epicsConvert.c*/

#include <math.h>
#include <float.h>
#include <limits.h>

#define epicsExportSharedSymbols
#include "epicsConvert.h"
#include "cantProceed.h"

epicsShareFunc float epicsConvertDoubleToFloat(double value)
{
    float rtnvalue;

    if(value==0.0) {
        return(0.0);
    } else {
        double abs = fabs(value);

        if(abs>=FLT_MAX) {
            if(value>0.0) rtnvalue = FLT_MAX; else rtnvalue = -FLT_MAX;
        } else if(abs<=FLT_MIN) {
            if(value>0.0) rtnvalue = FLT_MIN; else rtnvalue = -FLT_MIN;
        } else {
            rtnvalue = (float)value;
        }
    }
    return rtnvalue;
}
