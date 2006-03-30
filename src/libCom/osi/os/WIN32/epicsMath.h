/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef epicsMathh
#define epicsMathh

#include <math.h>
#include <float.h>

#define finite(D) _finite(D)
#define isnan(D) _isnan(D)
#define isinf(D) ( !_finite(D) && !_isnan(D) ) 

#endif /* epicsMathh */
