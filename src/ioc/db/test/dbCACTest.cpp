/*************************************************************************\
* Copyright (c) 2015 Brookhaven Science Assoc. as operator of Brookhaven
*               National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/
/*
 * Part of dbCaLinkTest, compiled seperately to avoid
 * dbAccess.h vs. db_access.h conflicts
 */

#include <stdio.h>

#include <vector>
#include <stdexcept>

#include <epicsEvent.h>

#include "epicsUnitTest.h"

#include "cadef.h"

#define testECA(OP) if((OP)!=ECA_NORMAL) {testAbort("%s", #OP);} else {testPass("%s", #OP);}

void putgetarray(chid chanid, double first, size_t count)
{
    testDiag("putgetarray(%f,%u)", first, (unsigned)count);

    std::vector<double> buf(count);
    for(size_t i=0; i<count ;i++)
        buf[i] = first + i*1.0;

    testDiag("Put");

    testECA(ca_array_put(DBR_DOUBLE, count, chanid, &buf[0]));

    testECA(ca_pend_io(1.0));

    testDiag("Get");

    std::vector<double> buf2(count);

    testECA(ca_array_get(DBR_DOUBLE, count, chanid, &buf2[0]));

    testECA(ca_pend_io(1.0));

    for(size_t i=0; i<count ;i++)
        testOk(buf[i]==buf2[i], "%f == %f", buf[i], buf2[i]);
}



struct CATestContext
{
    CATestContext()
    {
        if(ca_context_create(ca_enable_preemptive_callback)!=ECA_NORMAL)
            throw std::runtime_error("Failed to create CA context");
    }
    ~CATestContext()
    {
        ca_context_destroy();
    }
};

extern "C"
void dbCaLinkTest_testCAC(void)
{
    try {
        CATestContext ctxt;
        chid chanid = 0;
        testECA(ca_create_channel("target1", NULL, NULL, 0, &chanid));
        testECA(ca_pend_io(1.0));
        putgetarray(chanid, 1.0, 1);
        putgetarray(chanid, 2.0, 2);
        // repeat to ensure a cache hit in dbContextReadNotifyCacheAllocator
        putgetarray(chanid, 2.0, 2);
        putgetarray(chanid, 5.0, 5);

        testECA(ca_clear_channel(chanid));
    }catch(std::exception& e){
        testAbort("Unexpected exception in testCAC: %s", e.what());
    }
}
