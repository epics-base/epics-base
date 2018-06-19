/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
// epicsAlgorithm.h
//	Authors: Jeff Hill & Andrew Johnson

#ifndef __EPICS_ALGORITHM_H__
#define __EPICS_ALGORITHM_H__

#include "epicsMath.h"

// The C++ standard only requires types to be less-than comparable, so
// the epicsMin and epicsMax templates only use operator <

// epicsMin

template <class T>
inline const T& epicsMin (const T& a, const T& b)
{
    return (b < a) ? b : a;
}

// If b is a NaN the above template returns a, but should return NaN.
// These specializations ensure that epicsMin(x,NaN) == NaN

template <>
inline const float& epicsMin (const float& a, const float& b)
{
    return (b < a) || isnan(b) ? b : a;
}

template <>
inline const double& epicsMin (const double& a, const double& b)
{
    return (b < a) || isnan(b) ? b : a;
}


// epicsMax

template <class T>
inline const T& epicsMax (const T& a, const T& b)
{
    return (a < b) ? b : a;
}

// If b is a NaN the above template returns a, but should return NaN.
// These specializations ensure that epicsMax(x,NaN) == NaN

template <>
inline const float& epicsMax (const float& a, const float& b)
{
    return (a < b) || isnan(b) ? b : a;
}

template <>
inline const double& epicsMax (const double& a, const double& b)
{
    return (a < b) || isnan(b) ? b : a;
}


// epicsSwap

template <class T>
inline void epicsSwap(T& a, T& b)
{
    T temp = a;
    a = b;
    b = temp;
}

#endif // __EPICS_ALGORITHM_H__
