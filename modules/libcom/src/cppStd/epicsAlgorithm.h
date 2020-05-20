/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/**
 * \file epicsAlgorithm.h
 * \author Jeff Hill & Andrew Johnson
 *
 * \brief Contains a few templates out of the C++ standard header algorithm
 *
 * \note The templates are provided here in a much smaller file. Standard algorithm
 * contains many templates for sorting and searching through C++ template containers
 * which are not used in EPICS. If all you need from there is std::min(),
 * std::max() and/or std::swap() your code may compile faster if you include
 * epicsAlgorithm.h and use epicsMin(), epicsMax() and epicsSwap() instead.
 *
 * The C++ standard only requires types to be less-than comparable, so
 * the epicsMin and epicsMax templates only use operator <.
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
