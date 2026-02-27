/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef epicsMathh
#define epicsMathh

#include <math.h>
#include <ieeefp.h>
#include <libComAPI.h>

#ifdef __cplusplus
extern "C" {
#endif

LIBCOM_API extern const float epicsNAN;
LIBCOM_API extern const float epicsINF;

#ifdef __cplusplus
}
#endif

static inline long long epicsLlround(double x)
{
    if(!isfinite(x)) return 0;
    return (x >= 0.0)
        ? (long long)floor(x + 0.5)
        : -(long long)floor(-x + 0.5);
}

#ifndef __cplusplus
#  define llround(x) epicsLlround((double)(x))
#endif

#endif /* epicsMathh */
