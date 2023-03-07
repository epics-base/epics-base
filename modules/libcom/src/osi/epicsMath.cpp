/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* epicsMath.cpp */

#include <epicsMath.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4723)
#endif

#ifndef NAN
static float makeNAN ( void )
{
    float a = 0, b = 0;
    return a / b;
}
#define NAN makeNAN()
#endif

#ifndef INFINITY
static float makeINF ( void )
{
    float a = 1, b = 0;
    return a / b;
}
#define INFINITY makeINF()
#endif

extern "C" {
const float epicsNAN = NAN;
const float epicsINF = INFINITY;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

