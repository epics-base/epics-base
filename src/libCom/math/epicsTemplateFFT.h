/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

//
//      Author: Jeff Hill
//      Date:   9-29-03
//

#ifndef epicsTemplateFFT_h
#define epicsTemplateFFT_h

#include <complex>

#include <epicsDSP.h>

//
// Simple one dimensional FFT computing template function
//
// vec[]    Input Sequence replaced by output sequence
// ln       Log base two of the number of point in vec
// inverse  Inverse fft if true
//
template < class T >
void epicsOneDimFFT ( std::complex < T > vec[], 
        const unsigned ln, bool inverse )
{ 
    static const T  one = 1.0;
    static const T  four = 4.0;
    static const T  PI = four * atan ( one );
    const unsigned n = 1u << ln;
    const unsigned nv2 = n >> 1u;
    
    {
        unsigned j = 1u;
    
        for ( unsigned i = 1; i < n; i++ ) {
            if ( i < j ) {
                std::complex < T > t = vec[i - 1];
                vec[i - 1] = vec[j - 1];
                vec[j - 1] = t;
            }
            unsigned k = nv2;
            while ( k < j ) {
              j = j - k;
              k = k >> 1u;
            }
            j = j + k;
        }
    }

    const T sign = inverse ? 1.0 : -1.0;
    for ( unsigned l = 1; l <= ln; l++ ) {
        const unsigned le = 1u << l;
        const unsigned le1 = le >> 1u;
        const T radians = sign * PI / le1;
        const std::complex < T > w = std::polar ( one, radians );
        std::complex < T > u ( 1, 0 );
        
        for ( unsigned m = 1u; m <= le1; m++ ) {
            for ( unsigned i = m - 1; i < n; i = i + le ) {
                unsigned ip = i + le1;
                std::complex < T > t0 = vec [ ip ] * u;
                vec [ ip ] = vec [ i ] - t0;
                vec [ i  ] = vec [ i ] + t0;
            }
            u = u * w;
        }
    }
}

#endif // epicsTemplateFFT_h

