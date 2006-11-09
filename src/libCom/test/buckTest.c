/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <time.h>
#include <stdio.h>

#include "epicsAssert.h"
#include "bucketLib.h"
#include "testMain.h"

MAIN(buckTest)
{
        unsigned	id1;
        unsigned	id2;
        char            *pValSave1;
        char            *pValSave2;
        int             s;
        BUCKET          *pb;
        char            *pVal;
        unsigned        i;
        clock_t         start, finish;
        double          duration;
        const int       LOOPS = 500000;
 
        pb = bucketCreate(8);
        if(!pb){
                return -1;
        }
 
        id1 = 0x1000a432;
        pValSave1 = "fred";
        s = bucketAddItemUnsignedId(pb, &id1, pValSave1);
        assert (s == S_bucket_success);
 
        pValSave2 = "jane";
	id2 = 0x0000a432;
        s = bucketAddItemUnsignedId(pb, &id2, pValSave2);
        assert (s == S_bucket_success);
 
        start = clock();
        for(i=0; i<LOOPS; i++){
                pVal = bucketLookupItemUnsignedId(pb, &id1);
                assert(pVal == pValSave1);
                pVal = bucketLookupItemUnsignedId(pb, &id1);
                assert(pVal == pValSave1);
                pVal = bucketLookupItemUnsignedId(pb, &id1);
                assert(pVal == pValSave1);
                pVal = bucketLookupItemUnsignedId(pb, &id1);
                assert(pVal == pValSave1);
                pVal = bucketLookupItemUnsignedId(pb, &id1);
                assert(pVal == pValSave1);
                pVal = bucketLookupItemUnsignedId(pb, &id1);
                assert(pVal == pValSave1);
                pVal = bucketLookupItemUnsignedId(pb, &id1);
                assert(pVal == pValSave1);
                pVal = bucketLookupItemUnsignedId(pb, &id1);
                assert(pVal == pValSave1);
                pVal = bucketLookupItemUnsignedId(pb, &id1);
                assert(pVal == pValSave1);
                pVal = bucketLookupItemUnsignedId(pb, &id2);
                assert(pVal == pValSave2);
        }
        finish = clock();
 
        duration = finish-start;
        duration = duration/CLOCKS_PER_SEC;
        printf("It took %15.10f total sec\n", duration);
        duration = duration/LOOPS;
        duration = duration/10;
        duration = duration * 1e6;
        printf("It took %15.10f u sec per hash lookup\n", duration);
 
        bucketShow(pb);
 
        return S_bucket_success;
}

