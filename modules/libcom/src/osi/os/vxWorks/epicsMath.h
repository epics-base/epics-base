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
#include <private/mathP.h>
#include <shareLib.h>

/* private/mathP.h defines NAN as 4, and uses its value in the
 * isNan() macro.  We need mathP.h for isInf(), but can create
 * our own isnan() test.  epicsMath.cpp requires that NAN either
 * be undef or yield the NaN value, so this solves the issue.
 */
#undef NAN

#define isnan(D) (!(D == D))
#define isinf(D) isInf(D)
#define finite(D) (!isnan(D) && !isInf(D))

#ifdef __cplusplus
extern "C" {
#endif

epicsShareExtern float epicsNAN;
epicsShareExtern float epicsINF;

#ifdef __cplusplus
}
#endif

#endif /* epicsMathh */
