/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *      Author: Jeff Hill
 *      Date:   9-29-03
 */

#ifndef epicsDSP_h
#define epicsDSP_h

#ifdef __cplusplus

#if !defined ( __GNUC__ ) || ( __GNUC__ > 2 || ( __GNUC__ == 2 && __GNUC_MINOR__ >= 95 ) )
    namespace std {
        template < class T > class complex;
    }
#else
    template < class T > class complex;
#endif

//
// One dimensional FFT computing template function
//
// vec[]    Input Sequence replaced by output sequence
// ln       Log base two of the number of point in vec
// inverse  Inverse fft if true
//
template < class T >
void epicsOneDimFFT ( std::complex < T > vec[], 
        const unsigned ln, bool inverse );

#endif /* __cplusplus */

#endif /* epicsDSP_h */

