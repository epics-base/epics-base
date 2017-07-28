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
#include <float.h>
#include <shareLib.h>

#ifndef finite
#define finite(D) _finite(D)
#endif

#ifndef isnan
#define isnan(D) _isnan(D)
#endif

#ifndef isinf
#define isinf(D) ( !_finite(D) && !_isnan(D) ) 
#endif

#ifdef __cplusplus
extern "C" {
#endif

epicsShareExtern float epicsNAN;
epicsShareExtern float epicsINF;

#ifdef __cplusplus
}
#endif

#endif /* epicsMathh */
