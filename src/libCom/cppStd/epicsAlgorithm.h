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
