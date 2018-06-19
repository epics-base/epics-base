/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

//
// simple type safe inline template functions to replace 
// the min() and max() macros
//

#ifndef tsMinMaxh
#define tsMinMaxh

template <class T> 
inline const T & tsMax ( const T & a, const T & b )
{
    return ( a > b ) ? a : b;
}
 
template <class T> 
inline const T & tsMin ( const T & a, const T & b )
{
    return ( a < b ) ? a : b;
}

#endif // tsMinMaxh
