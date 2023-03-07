/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne, LLC as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_epicsMath_H
#define INC_epicsMath_H

#include <math.h>
#include <ieeefp.h>
#include <libComAPI.h>

#ifndef isinf
#  define isinf(x) (((x)==(x)) && !finite((x)))
/* same as (!isnan(x) && !finite(x)) */
#endif

#ifndef isnan
#  define isnan(x) ((x) != (x))
#endif

#ifdef __cplusplus
extern "C" {
#endif

LIBCOM_API extern const float epicsNAN;
LIBCOM_API extern const float epicsINF;

#ifdef __cplusplus
}
#endif

#endif /* INC_epicsMath_H */
