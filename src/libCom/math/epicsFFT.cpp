
/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

//
//      Simple fft computing functions
//
//      Author: Jeff Hill
//      Date:            9-29-03
//

#include <complex>

#define epicsExportSharedSymbols
#include <shareLib.h>
#include <epicsTemplateFFT.h>

// template epicsShareFunc void epicsShareAPI epicsOneDimFFT ( 
//     std::complex < float > vec[], 
//     const unsigned ln, bool inverse );

template epicsShareFunc void epicsShareAPI epicsOneDimFFT ( 
    std::complex < double > vec[], 
    const unsigned ln, bool inverse );
        
