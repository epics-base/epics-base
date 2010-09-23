/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef epicsMathh
#define epicsMathh

#include <math.h>
#include <shareLib.h>

#define finite(x) isfinite(x)

#ifdef __cplusplus
extern "C" {
#endif

epicsShareExtern float epicsNAN;
epicsShareExtern float epicsINF;

#ifdef __cplusplus
}
#endif

#endif /* epicsMathh */
