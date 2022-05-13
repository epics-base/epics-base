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
#include <libComAPI.h>

#ifdef __cplusplus

#if __cplusplus>=201103L
#include <cmath>

#if __GLIBCXX__>20160427
using std::isfinite;
using std::isinf;
using std::isnan;
using std::isnormal;
#endif
#endif /* c++11 */

extern "C" {
#endif

#ifdef isfinite
#  undef finite
#  define finite(x) isfinite((double)(x))
#endif

LIBCOM_API extern const float epicsNAN;
LIBCOM_API extern const float epicsINF;

#ifdef __cplusplus
}
#endif

#endif /* epicsMathh */
