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

template <class T>
inline const T& epicsMax (const T& a, const T& b)
{
    return (a<b) ? b : a;
}

template <class T>
inline const T& epicsMin (const T& a, const T& b)
{
    return (a<b) ? a : b;
}

template <class T>
inline void epicsSwap(T& a, T& b)
{
    T temp = a;
    a = b;
    b = temp;
}

#endif // __EPICS_ALGORITHM_H__
