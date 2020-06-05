/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/**
 * \file epicsAlgorithm.h
 * \author Jeff Hill & Andrew Johnson
 *
 * \brief Contains a few templates out of the C++ standard header algorithm
 *
 * \deprecated Use std::min()/max()/swap() in new code
 */

#ifndef __EPICS_ALGORITHM_H__
#define __EPICS_ALGORITHM_H__

#include "epicsMath.h"


/**
 * Returns the smaller of a or b compared using a<b.
 *
 */
template <class T>
inline const T& epicsMin (const T& a, const T& b)
{
    return (b < a) ? b : a;
}

/**
 * Returns the smaller of a or b compared using a<b.
 *
 * \note If b is a NaN the above template returns a, but should return NaN.
 * These specializations ensure that epicsMin(x,NaN) == NaN
 */
template <>
inline const float& epicsMin (const float& a, const float& b)
{
    return (b < a) || isnan(b) ? b : a;
}

/**
 * Returns the smaller of a or b compared using a<b.
 *
 * \note If b is a NaN the above template returns a, but should return NaN.
 * These specializations ensure that epicsMin(x,NaN) == NaN
 */
template <>
inline const double& epicsMin (const double& a, const double& b)
{
    return (b < a) || isnan(b) ? b : a;
}

/**
 * Returns the larger of a or b compared using a<b.
 *
 */
template <class T>
inline const T& epicsMax (const T& a, const T& b)
{
    return (a < b) ? b : a;
}

/**
 * Returns the larger of a or b compared using a<b.
 *
 * \note If b is a NaN the above template returns a, but should return NaN.
 * These specializations ensure that epicsMax(x,NaN) == NaN
 */
template <>
inline const float& epicsMax (const float& a, const float& b)
{
    return (a < b) || isnan(b) ? b : a;
}

/**
 * Returns the larger of a or b compared using a<b.
 *
 * \note If b is a NaN the above template returns a, but should return NaN.
 * These specializations ensure that epicsMax(x,NaN) == NaN
 */
template <>
inline const double& epicsMax (const double& a, const double& b)
{
    return (a < b) || isnan(b) ? b : a;
}

/**
 * Swaps the values of a and b.
 *
 * \note The data type must support both copy-construction and assignment.
 *
 */
template <class T>
inline void epicsSwap(T& a, T& b)
{
    T temp = a;
    a = b;
    b = temp;
}

#endif // __EPICS_ALGORITHM_H__
