/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef epicsMathh
#define epicsMathh

#include <math.h>
#include <libComAPI.h>

#define finite(x) isfinite(x)

#ifdef __cplusplus
extern "C" {
#endif

LIBCOM_API extern const float epicsNAN;
LIBCOM_API extern const float epicsINF;

#ifdef __cplusplus
}
#endif

#endif /* epicsMathh */
