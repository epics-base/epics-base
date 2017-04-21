/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef epicsMathh
#define epicsMathh

#include <math.h>
#include <shareLib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef isfinite
#  undef finite
#  define finite(x) isfinite((double)(x))
#endif

epicsShareExtern float epicsNAN;
epicsShareExtern float epicsINF;

#ifdef __cplusplus
}
#endif

#endif /* epicsMathh */
