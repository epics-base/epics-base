/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
// epicsAlgorithmTest.cpp
//	Authors: Jeff Hill & Andrew Johnson

#include <assert.h>
#include <stdio.h>

#include "epicsAlgorithm.h"

#if defined(vxWorks) || defined(__rtems__)
  #define MAIN epicsAlgorithm
  extern "C" int MAIN(int /*argc*/, char* /*argv[]*/);
#else
  #define MAIN main
#endif


int MAIN(int /*argc*/, char* /*argv[]*/)
{
    float f1 = 3.3f;
    float f2 = 3.4f;
    float f3;	
    
    f3 = epicsMin(f1,f2);
    assert(f3==f1);
    
    f3 = epicsMax(f1,f2);
    assert(f3==f2);
    
    epicsSwap(f1,f2);
    assert(f1==3.4f);
    assert(f2==3.3f);
    
    int i1 = 3;
    int i2 = 4;
    int i3;	
    
    i3 = epicsMin(i1,i2);
    assert(i3==i1);
    
    i3 = epicsMax(i1,i2);
    assert(i3==i2);
    
    epicsSwap(i1,i2);
    assert(i1==4);
    assert(i2==3);
    
    puts("epicsMin, epicsMax and epicsSwap tested OK.");
    return 0;
}

