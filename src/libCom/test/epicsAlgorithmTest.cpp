// epicsAlgorithmTest.cpp
//	Authors: Jeff Hill & Andrew Johnson

#include <assert.h>

#include "epicsAlgorithm.h"

#ifdef vxWorks
  #define MAIN epicsAlgorithm
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
    
    return 0;
}

